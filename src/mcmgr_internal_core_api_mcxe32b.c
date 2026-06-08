/*
 * Copyright 2026 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * MCXE32B dual Cortex-M7 mcmgr porting layer.
 *
 * The build target core is selected at compile time by the device series macro:
 *   MCXE32B_cm7_core0_SERIES -> M7_0 = PRIMARY core   = MU2_A side = kMCMGR_Core0
 *   MCXE32B_cm7_core1_SERIES -> M7_1 = SECONDARY core = MU2_B side = kMCMGR_Core1
 *
 * Core0 (M7_0) always boots first (SBAF requirement: M7_1 cannot be released as
 * primary without M7_0). The primary core (core0) releases the secondary core
 * (core1) from reset via the MC_ME PRTN0_CORE1 interface.
 *
 * MU2_A/MU2_B each expose dedicated NVIC lines on MCXE32B (TX, RX, general purpose):
 *   MU2_A_TX_IRQn / MU2_A_RX_IRQn / MU2_A_IRQn -> primary   (M7_0, MU2_A side, kMCMGR_Core0)
 *   MU2_B_TX_IRQn / MU2_B_RX_IRQn / MU2_B_IRQn -> secondary (M7_1, MU2_B side, kMCMGR_Core1)
 *
 * Note: the kMCMGR_CoreX enum ids stay glued to the physical core (kMCMGR_Core0
 * = M7_0, kMCMGR_Core1 = M7_1); only the primary/secondary role labels and the
 * MC_ME release target follow the inverted boot order.
 */

#include "mcmgr.h"
#include "mcmgr_internal_core_api.h"
#include "fsl_device_registers.h"
#include "fsl_mu.h"


/*
 * On MCXE32B both cores define FSL_FEATURE_MU_SIDE_A and FSL_FEATURE_MU_SIDE_B
 * because both are CM7 and can access either MU side. Use the series macro
 * (set by the device header via MCXE32B_cm7_core0_SERIES / _core1_SERIES) to
 * differentiate at compile time.
 */
#if defined(MCXE32B_cm7_core0_SERIES)
/* Building for M7_0 = PRIMARY core, MU2_A side, kMCMGR_Core0 */
#define MCMGR_BUILD_FOR_CORE_0
#elif defined(MCXE32B_cm7_core1_SERIES)
/* Building for M7_1 = SECONDARY core, MU2_B side, kMCMGR_Core1 */
#define MCMGR_BUILD_FOR_CORE_1
#else
#error "MCXE32B mcmgr: unrecognized series macro - expected MCXE32B_cm7_core0_SERIES or MCXE32B_cm7_core1_SERIES"
#endif

/* MU TR/RR MCMGR_MU_CHANNEL is managed by MCMGR */
#define MU_RX_ISR_Handler(x)     MU_RX_ISR(x)
#define MU_RX_ISR(number)        MU_Rx##number##FullFlagISR
#define mcmgr_mu_channel_handler MU_RX_ISR_Handler(MCMGR_MU_CHANNEL)
#define MU_RX_ISR_FLAG_Mask(x)   MU_RX_ISR_FLAG(x)
#define MU_RX_ISR_FLAG(number)   kMU_Rx##number##FullInterruptEnable
#define mcmgr_mu_channel_flag    MU_RX_ISR_FLAG_Mask(MCMGR_MU_CHANNEL)

volatile mcmgr_core_context_t s_mcmgrCoresContext[MCMGR_CORECOUNT] = {
    {.state = kMCMGR_ResetCoreState, .startupData = 0},
    {.state = kMCMGR_ResetCoreState, .startupData = 0}};

/* Index 0 = kMCMGR_Core0 = M7_0 = primary
 * Index 1 = kMCMGR_Core1 = M7_1 = secondary */
static const mcmgr_core_info_t s_mcmgrCores[MCMGR_CORECOUNT] = {
    {.coreType = kMCMGR_CoreTypeCortexM7, .coreName = "Primary"},
    {.coreType = kMCMGR_CoreTypeCortexM7, .coreName = "Secondary"}};

const mcmgr_system_info_t g_mcmgrSystem = {
    .coreCount = MCMGR_CORECOUNT, .memRegCount = MCMGR_MEMREGCOUNT, .cores = s_mcmgrCores};

#if defined(MCMGR_BUILD_FOR_CORE_0)
/*
 * Gate on the inter-core MU2 peripheral clock in MC_ME before any MU2 access.
 * Both MU2 sides are gated by PRTN0_COFB1_CLKEN:
 *   MU2_A -> REQ46 (block 46), MU2_B -> REQ47 (block 47).
 * Enabling both sides is system-wide and idempotent, so the secondary core's
 * call simply finds the clocks already on and the STAT poll exits immediately.
 */
static mcmgr_status_t mcxe32b_enable_mu2_clock(void)
{
#if defined(MCMGR_BUSY_POLL_COUNT) && (MCMGR_BUSY_POLL_COUNT > 0)
    uint32_t poll_count = MCMGR_BUSY_POLL_COUNT;
#endif

    MC_ME->PRTN0_COFB1_CLKEN |=
        (MC_ME_PRTN0_COFB1_CLKEN_REQ46_MASK | MC_ME_PRTN0_COFB1_CLKEN_REQ47_MASK);
    MC_ME->PRTN0_PUPD |= MC_ME_PRTN0_PUPD_PCUD_MASK;
    MC_ME->CTL_KEY = MC_ME_CTL_KEY_KEY(0x5AF0U);
    MC_ME->CTL_KEY = MC_ME_CTL_KEY_KEY(0xA50FU);
    while ((MC_ME_PRTN0_COFB1_STAT_BLOCK46_MASK | MC_ME_PRTN0_COFB1_STAT_BLOCK47_MASK) !=
           (MC_ME->PRTN0_COFB1_STAT &
            (MC_ME_PRTN0_COFB1_STAT_BLOCK46_MASK | MC_ME_PRTN0_COFB1_STAT_BLOCK47_MASK)))
    {
#if defined(MCMGR_BUSY_POLL_COUNT) && (MCMGR_BUSY_POLL_COUNT > 0)
        if ((--poll_count) == 0u)
        {
            return kStatus_MCMGR_Error;
        }
#endif
    }

    return kStatus_MCMGR_Success;
}
#endif


static mcmgr_status_t mcmgr_platform_init_internal_early(mcmgr_core_t coreNum)
{
    mcmgr_core_t target_core;

#if defined(MCMGR_BUILD_FOR_CORE_0)
    /* Primary core gates on both MU2 sides (REQ46=MU2_A, REQ47=MU2_B) before
     * releasing the secondary; the secondary inherits the running clock. */
    mcmgr_status_t clk_status = mcxe32b_enable_mu2_clock();
    if (clk_status != kStatus_MCMGR_Success)
    {
        return clk_status;
    }
#endif

    switch (coreNum)
    {
#if defined(MCMGR_BUILD_FOR_CORE_0)
        case kMCMGR_Core0:
            /* primary (M7_0, MU2_A side); notify the other (secondary) core */
            target_core = kMCMGR_Core1;
            MU_Init(MU2_A);
            break;
#endif
#if defined(MCMGR_BUILD_FOR_CORE_1)
        case kMCMGR_Core1:
            /* secondary (M7_1, MU2_B side); notify the other (primary) core */
            target_core = kMCMGR_Core0;
            MU_Init(MU2_B);
            break;
#endif
        default:
            return kStatus_MCMGR_Error;
    }

    return mcmgr_trigger_event_internal(target_core, kMCMGR_RemoteCoreUpEvent, 0U, false);
}

mcmgr_status_t mcmgr_platform_init_internal(mcmgr_core_t coreNum)
{
    mcmgr_status_t status = mcmgr_platform_init_internal_early(coreNum);
    if (status != kStatus_MCMGR_Success)
    {
        return status;
    }

#if defined(MCMGR_BUILD_FOR_CORE_0)
    /* Primary core (MU2_A side): enable the dedicated MU2_A NVIC lines
     * (TX, RX and general purpose) so MU2_A events reach this core. */
    MU_EnableInterrupts(MU2_A, (uint32_t)mcmgr_mu_channel_flag);

#if (defined(FSL_FEATURE_MU_HAS_RESET_ASSERT_INT) && FSL_FEATURE_MU_HAS_RESET_ASSERT_INT)
    MU_EnableInterrupts(MU2_A, (uint32_t)kMU_ResetAssertInterruptEnable);
#endif

    NVIC_SetPriority(MU2_A_TX_IRQn, 2);
    NVIC_SetPriority(MU2_A_RX_IRQn, 2);
    NVIC_SetPriority(MU2_A_IRQn, 2);
    NVIC_EnableIRQ(MU2_A_TX_IRQn);
    NVIC_EnableIRQ(MU2_A_RX_IRQn);
    NVIC_EnableIRQ(MU2_A_IRQn);

#elif defined(MCMGR_BUILD_FOR_CORE_1)
    /* Secondary core (MU2_B side): enable the dedicated MU2_B NVIC lines
     * (TX, RX and general purpose) so MU2_B events reach this core. */
    MU_EnableInterrupts(MU2_B, (uint32_t)mcmgr_mu_channel_flag);

#if (defined(FSL_FEATURE_MU_HAS_RESET_ASSERT_INT) && FSL_FEATURE_MU_HAS_RESET_ASSERT_INT)
    MU_EnableInterrupts(MU2_B, (uint32_t)kMU_ResetAssertInterruptEnable);
#endif

    NVIC_SetPriority(MU2_B_TX_IRQn, 2);
    NVIC_SetPriority(MU2_B_RX_IRQn, 2);
    NVIC_SetPriority(MU2_B_IRQn, 2);
    NVIC_EnableIRQ(MU2_B_TX_IRQn);
    NVIC_EnableIRQ(MU2_B_RX_IRQn);
    NVIC_EnableIRQ(MU2_B_IRQn);

#endif

    return kStatus_MCMGR_Success;
}


mcmgr_status_t mcmgr_start_core_internal(mcmgr_core_t coreNum, void *bootAddress)
{
    /* Only the primary (core0) can start the secondary (core1) */
    if (coreNum != kMCMGR_Core1)
    {
        return kStatus_MCMGR_Error;
    }
    if (bootAddress == ((void *)0))
    {
        return kStatus_MCMGR_Error;
    }

#if defined(MCMGR_BUILD_FOR_CORE_0)
    /*
     * Release M7_1 (secondary, kMCMGR_Core1) from reset via the MC_ME
     * PRTN0_CORE1 interface. This is the standard S32K3/MCXE dual-core
     * start sequence documented in the MCXE32B Reference Manual (MC_ME chapter):
     *
     *  1. Write the secondary core reset-vector base address to PRTN0_CORE1_ADDR.
     *     The core fetches its IVT from this address after reset release.
     *  2. Set CCE=1 in PRTN0_CORE1_PCONF to request the core clock.
     *  3. Set CCUPD=1 in PRTN0_CORE1_PUPD to flag a pending update.
     *  4. Write CTL_KEY = 0x5AF0 (first phase) then 0xA50F (second phase) to
     *     commit all pending PCONF/PUPD changes and release the core.
     *  5. Poll PRTN0_CORE1_PUPD.CCUPD until 0 to confirm the pending update has
     *     been consumed and the core has been released from reset.
     *
     * PRTN0_CORE0 maps to M7_0 (primary,   kMCMGR_Core0) - already running.
     * PRTN0_CORE1 maps to M7_1 (secondary, kMCMGR_Core1).
     */
    MC_ME->PRTN0_CORE1_ADDR = (uint32_t)bootAddress & MC_ME_PRTN0_CORE1_ADDR_ADDR_MASK;
    MC_ME->PRTN0_CORE1_PCONF |= MC_ME_PRTN0_CORE1_PCONF_CCE_MASK;
    MC_ME->PRTN0_CORE1_PUPD  |= MC_ME_PRTN0_CORE1_PUPD_CCUPD_MASK;
    MC_ME->CTL_KEY = MC_ME_CTL_KEY_KEY(0x5AF0U);
    MC_ME->CTL_KEY = MC_ME_CTL_KEY_KEY(0xA50FU);
    /* Wait for the pending update to be consumed (CCUPD self-clears when MC_ME
     * has committed the PCONF change and released the core). This is the same
     * handshake used by the verified APP_BootCore1() in the MU driver example. */
#if defined(MCMGR_BUSY_POLL_COUNT) && (MCMGR_BUSY_POLL_COUNT > 0)
    uint32_t poll_count = MCMGR_BUSY_POLL_COUNT;
#endif
    while (0U != (MC_ME->PRTN0_CORE1_PUPD & MC_ME_PRTN0_CORE1_PUPD_CCUPD_MASK))
    {
#if defined(MCMGR_BUSY_POLL_COUNT) && (MCMGR_BUSY_POLL_COUNT > 0)
        if ((--poll_count) == 0u)
        {
            return kStatus_MCMGR_Error;
        }
#endif
    }
#endif

    return kStatus_MCMGR_Success;
}

mcmgr_status_t mcmgr_get_startup_data_internal(mcmgr_core_t coreNum, uint32_t *startupData)
{
    if (coreNum != kMCMGR_Core1)
    {
        return kStatus_MCMGR_Error;
    }
    if (startupData == ((void *)0))
    {
        return kStatus_MCMGR_Error;
    }

    if (s_mcmgrCoresContext[coreNum].state >= kMCMGR_RunningCoreState)
    {
        *startupData = s_mcmgrCoresContext[coreNum].startupData;
        return kStatus_MCMGR_Success;
    }
    else
    {
        return kStatus_MCMGR_NotReady;
    }
}

mcmgr_status_t mcmgr_stop_core_internal(mcmgr_core_t coreNum)
{
    /* Only the primary (core0) can stop the secondary (core1) */
    if (coreNum != kMCMGR_Core1)
    {
        return kStatus_MCMGR_Error;
    }

#if defined(MCMGR_BUILD_FOR_CORE_0)
    /*
     * Stop M7_1 (secondary, kMCMGR_Core1) by gating its core clock off via the
     * MC_ME PRTN0_CORE1 interface. This is the inverse of the release sequence
     * in mcmgr_start_core_internal():
     *
     *  1. Clear CCE in PRTN0_CORE1_PCONF to request the core clock be removed.
     *  2. Set CCUPD=1 in PRTN0_CORE1_PUPD to flag a pending update.
     *  3. Write CTL_KEY = 0x5AF0 (first phase) then 0xA50F (second phase) to
     *     commit the pending PCONF/PUPD change and halt the core.
     *  4. Poll PRTN0_CORE1_PUPD.CCUPD until 0 to confirm the pending update has
     *     been consumed and the core clock has been gated off.
     *
     * On MCXE32B Stop/Start is a clock-gate suspend/resume, not a reset: gating
     * the core clock only halts M7_1 (PC, registers and RAM are retained). A
     * subsequent MCMGR_StartCore() re-enables the clock and the secondary RESUMES
     * from where it was halted - it does NOT restart from its reset vector,
     * because this SoC has no per-core software reset (MC_RGM exposes only
     * system/destructive reset domains, no PRTN0_CORE1 reset).
     */
    MC_ME->PRTN0_CORE1_PCONF &= ~MC_ME_PRTN0_CORE1_PCONF_CCE_MASK;
    MC_ME->PRTN0_CORE1_PUPD  |= MC_ME_PRTN0_CORE1_PUPD_CCUPD_MASK;
    MC_ME->CTL_KEY = MC_ME_CTL_KEY_KEY(0x5AF0U);
    MC_ME->CTL_KEY = MC_ME_CTL_KEY_KEY(0xA50FU);
    /* Wait for the pending update to be consumed (CCUPD self-clears when MC_ME
     * has committed the PCONF change and halted the core clock). */
#if defined(MCMGR_BUSY_POLL_COUNT) && (MCMGR_BUSY_POLL_COUNT > 0)
    uint32_t poll_count = MCMGR_BUSY_POLL_COUNT;
#endif
    while (0U != (MC_ME->PRTN0_CORE1_PUPD & MC_ME_PRTN0_CORE1_PUPD_CCUPD_MASK))
    {
#if defined(MCMGR_BUSY_POLL_COUNT) && (MCMGR_BUSY_POLL_COUNT > 0)
        if ((--poll_count) == 0u)
        {
            return kStatus_MCMGR_Error;
        }
#endif
    }
#endif

    return kStatus_MCMGR_Success;
}


mcmgr_status_t mcmgr_get_core_property_internal(mcmgr_core_t coreNum,
                                                 mcmgr_core_property_t property,
                                                 void *value,
                                                 uint32_t *length)
{
    return kStatus_MCMGR_NotImplemented;
}

mcmgr_core_t mcmgr_get_current_core_internal(void)
{
#if defined(MCMGR_BUILD_FOR_CORE_0)
    return kMCMGR_Core0;
#elif defined(MCMGR_BUILD_FOR_CORE_1)
    return kMCMGR_Core1;
#endif
}

mcmgr_status_t mcmgr_trigger_event_internal(mcmgr_core_t coreNum, mcmgr_event_type_t type, uint16_t eventData, bool forcedWrite)
{
    /* coreNum unused on a two-core platform; ifdefs select the correct MU side */
    (void)coreNum;

    /* Pack type and event data into the 32-bit MU wire word: type in [31:16], eventData in [15:0].
     * The RX handler (mcmgr_mu_channel_handler) unpacks the same way. */
    uint32_t remoteData = (((uint32_t)type) << 16U) | (uint32_t)eventData;

    if (false == forcedWrite)
    {
        /* Blocking send */
#if defined(MCMGR_BUILD_FOR_CORE_0)
        MU_SendMsg(MU2_A, MCMGR_MU_CHANNEL, remoteData);
#elif defined(MCMGR_BUILD_FOR_CORE_1)
        MU_SendMsg(MU2_B, MCMGR_MU_CHANNEL, remoteData);
#endif
    }
    else
    {
        /* Non-blocking send */
#if defined(MCMGR_BUILD_FOR_CORE_0)
        MU_SendMsgNonBlocking(MU2_A, MCMGR_MU_CHANNEL, remoteData);
#elif defined(MCMGR_BUILD_FOR_CORE_1)
        MU_SendMsgNonBlocking(MU2_B, MCMGR_MU_CHANNEL, remoteData);
#endif
    }

    /*
     * Writing the MU TX register sets the RX-full flag on the peer side, which
     * raises the peer's dedicated MU2 RX NVIC line directly. No additional
     * doorbell is required on MCXE32B now that MU2 exposes dedicated NVIC lines.
     */

    return kStatus_MCMGR_Success;
}


/*!
 * @brief RX channel ISR - called from MU ISR when data arrives on MCMGR_MU_CHANNEL
 */
void mcmgr_mu_channel_handler(MU_Type *base, mcmgr_core_t coreNum)
{
    uint32_t data;
    uint16_t eventType;
    uint16_t eventData;

    data = MU_ReceiveMsgNonBlocking(base, MCMGR_MU_CHANNEL);

    if (0U != data)
    {
        eventType = (uint16_t)(data >> 16u);
        eventData = (uint16_t)(data & 0x0000FFFFu);

        if (((mcmgr_event_type_t)eventType >= kMCMGR_RemoteCoreUpEvent) &&
            ((mcmgr_event_type_t)eventType < kMCMGR_EventTableLength))
        {
            if (MCMGR_eventTable[(mcmgr_event_type_t)eventType].callback != ((void *)0))
            {
                MCMGR_eventTable[(mcmgr_event_type_t)eventType].callback(
                    coreNum, eventData,
                    MCMGR_eventTable[(mcmgr_event_type_t)eventType].callbackData);
            }
        }
    }
}

#if defined(MCMGR_HANDLE_EXCEPTIONS) && (MCMGR_HANDLE_EXCEPTIONS == 1)
void DefaultISR(void)
{
    mcmgr_core_t target_core;
    uint32_t exceptionNumber = __get_IPSR();

#if defined(MCMGR_BUILD_FOR_CORE_0)
    target_core = kMCMGR_Core1;
#else
    target_core = kMCMGR_Core0;
#endif

    (void)MCMGR_TriggerEvent(target_core, kMCMGR_RemoteExceptionEvent, (uint16_t)exceptionNumber);
    for (;;)
    {
    }
}

void HardFault_Handler(void)
{
    DefaultISR();
}

void NMI_Handler(void)
{
    DefaultISR();
}

void MemManage_Handler(void)
{
    DefaultISR();
}

void BusFault_Handler(void)
{
    DefaultISR();
}

void UsageFault_Handler(void)
{
    DefaultISR();
}
#endif /* MCMGR_HANDLE_EXCEPTIONS */
