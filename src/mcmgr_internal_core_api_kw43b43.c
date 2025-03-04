/*
 * Copyright 2024-2025 NXP
 *
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "mcmgr.h"
#include "mcmgr_internal_core_api.h"
#include "fsl_device_registers.h"
#include "fsl_mu.h"
#include "board.h"

/* Count of cores in the system */
#define MCMGR_CORECOUNT 2

/* Count of memory regions in the system */
#define MCMGR_MEMREGCOUNT 2

/* MCMGR MU channel index - used for passing startupData */
#define MCMGR_MU_CHANNEL 3

/* MU TR/RR $MCMGR_MU_CHANNEL is managed by MCMGR */
#define MU_RX_ISR_Handler(x)     MU_RX_ISR(x)
#define MU_RX_ISR(number)        MU_Rx##number##FullFlagISR
#define mcmgr_mu_channel_handler MU_RX_ISR_Handler(MCMGR_MU_CHANNEL)
#define MU_RX_ISR_FLAG_Mask(x)   MU_RX_ISR_FLAG(x)
#define MU_RX_ISR_FLAG(number)   kMU_Rx##number##FullInterruptEnable
#define mcmgr_mu_channel_flag    MU_RX_ISR_FLAG_Mask(MCMGR_MU_CHANNEL)

volatile mcmgr_core_context_t s_mcmgrCoresContext[MCMGR_CORECOUNT] = {
    {.state = kMCMGR_ResetCoreState, .startupData = 0}, {.state = kMCMGR_ResetCoreState, .startupData = 0}};

/* Initialize structure with informations of all cores */
static const mcmgr_core_info_t s_mcmgrCores[MCMGR_CORECOUNT] = {
    {.coreType = kMCMGR_CoreTypeCortexM33, .coreName = "Main"},
    {.coreType = kMCMGR_CoreTypeCortexM33, .coreName = "Secondary"}};

const mcmgr_system_info_t g_mcmgrSystem = {
    .coreCount = MCMGR_CORECOUNT, .memRegCount = MCMGR_MEMREGCOUNT, .cores = s_mcmgrCores};

mcmgr_status_t mcmgr_early_init_internal(mcmgr_core_t coreNum)
{
    /* This function is intended to be called as close to the reset entry as possible,
       (within the startup sequence in SystemInitHook) to allow CoreUp event triggering.
       Avoid using uninitialized data here. */

    uint32_t flags;
    __attribute__((unused)) uint32_t data;
/* MUA clk enable */
#if defined(FSL_FEATURE_MU_SIDE_A)
    MU_Init(MU0_MUA);
    /* Clear all RX registers and status flags */
    for (uint32_t idx = 0U; idx < MU_RR_COUNT; idx++)
    {
        data = MU_ReceiveMsgNonBlocking(MU0_MUA, idx);
    }
    flags = MU_GetStatusFlags(MU0_MUA);
    MU_ClearStatusFlags(MU0_MUA, flags);
    /* Do not perform MU reset to avoid issues when debugging both CM33 and CM7 */
#endif
/* MUB clk enable */
#if defined(FSL_FEATURE_MU_SIDE_B)
    MU_Init(MU0_MUB);
    for (uint32_t idx = 0U; idx < MU_RR_COUNT; idx++)
    {
        data = MU_ReceiveMsgNonBlocking(MU0_MUB, idx);
    }
    flags = MU_GetStatusFlags(MU0_MUB);
    MU_ClearStatusFlags(MU0_MUB, flags);
#endif

    /* Trigger core up event here, core is starting! */
    return MCMGR_TriggerEvent(kMCMGR_RemoteCoreUpEvent, 0);
}

mcmgr_status_t mcmgr_late_init_internal(mcmgr_core_t coreNum)
{
#if defined(FSL_FEATURE_MU_SIDE_A)
    MU_EnableInterrupts(MU0_MUA, (uint32_t)mcmgr_mu_channel_flag);

#if (defined(FSL_FEATURE_MU_HAS_RESET_ASSERT_INT) && FSL_FEATURE_MU_HAS_RESET_ASSERT_INT)
    MU_EnableInterrupts(MU0_MUA, (uint32_t)kMU_ResetAssertInterruptEnable);
#endif

    NVIC_SetPriority(MU0_IRQn, 2);

    NVIC_EnableIRQ(MU0_IRQn);

#elif defined(FSL_FEATURE_MU_SIDE_B)
    MU_EnableInterrupts(MU0_MUB, (uint32_t)mcmgr_mu_channel_flag);

#if (defined(FSL_FEATURE_MU_HAS_RESET_ASSERT_INT) && FSL_FEATURE_MU_HAS_RESET_ASSERT_INT)
    MU_EnableInterrupts(MU0_MUB, (uint32_t)kMU_ResetAssertInterruptEnable);
#endif

    NVIC_SetPriority(MU0_IRQn, 2);

    NVIC_EnableIRQ(MU0_IRQn);

#endif

    return kStatus_MCMGR_Success;
}

mcmgr_status_t mcmgr_start_core_internal(mcmgr_core_t coreNum, void *bootAddress)
{
    __attribute__((unused)) volatile uint32_t result1, result2;

    if (coreNum != kMCMGR_Core1)
    {
        return kStatus_MCMGR_Error;
    }

#if defined(KW43B43ZC7_SERIES)
    GlikeyWriteEnable(GLIKEY, 0U);
#endif
#if 0
    *((uint32_t *)0x5019fd00) = 0x00060000;//clear bit18
    *((uint32_t *)0x5019fd00) = 0x00020000;
    *((uint32_t *)0x5019fd00) = 0x00010000;
    *((uint32_t *)0x5019fd04) = 0x00290000;
    *((uint32_t *)0x5019fd00) = 0x00020000;
    *((uint32_t *)0x5019fd04) = 0x00280000;
    *((uint32_t *)0x5019fd00) = 0x00000000;
#endif

    /* the FPGA ROM code makes the NBU boot from RAM and run a while(1) loop
     * so we need to put it in reset before changing the boot address to make
     * sure it will boot from the address we want it to */
    SECCON->CPU1_RESET_CTRL = 0xA;
    __DSB();

    SECCON->CPU1_CTRL = (uint32_t)(char *)bootAddress;

    SYSCON->CPU1_WAIT = 0;

#if defined(KW43B43ZC7_SERIES)
    GlikeyWriteEnable(GLIKEY, 0U);
#endif
#if 0
    *((uint32_t *)0x5019fd00) = 0x00060000;//clear bit18
    *((uint32_t *)0x5019fd00) = 0x00020000;
    *((uint32_t *)0x5019fd00) = 0x00010000;
    *((uint32_t *)0x5019fd04) = 0x00290000;
    *((uint32_t *)0x5019fd00) = 0x00020000;
    *((uint32_t *)0x5019fd04) = 0x00280000;
    *((uint32_t *)0x5019fd00) = 0x00000000;
#endif


    /* release CPU1 reset */
    SECCON->CPU1_RESET_CTRL = 0;
    /* pd_infra MRCC clk teal1 */
    SECCON->CPU1_DEBUG_EN_DP = SECCON_CPU1_DEBUG_EN_DP_CPU1_NIDEN(1) | SECCON_CPU1_DEBUG_EN_DP_CPU1_DBGEN(1);
    MRCC_0->MRCC_UTEAL_1_CLKSEL = MRCC_MRCC_UTEAL_1_CLKSEL_CC(1) | MRCC_MRCC_UTEAL_1_CLKSEL_RSTB(1) | MRCC_MRCC_UTEAL_1_CLKSEL_PR(1);

    uint32_t wdata;
    wdata = *((uint32_t *)0x501a3018)&0xdfffffff;
    *((uint32_t *)0x501a3018) = wdata;

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
    /* It is not allowed to stop the secondary core */
    return kStatus_MCMGR_NotImplemented;
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
#if defined(FSL_FEATURE_MU_SIDE_A)
    return kMCMGR_Core0;
#elif defined(FSL_FEATURE_MU_SIDE_B)
    return kMCMGR_Core1;
#endif
}

mcmgr_status_t mcmgr_trigger_event_internal(mcmgr_core_t coreNum, uint32_t remoteData, bool forcedWrite)
{
    /* When forcedWrite is false, execute the blocking call, i.e. wait until previously
       sent data is processed. Otherwise, run the non-blocking version of the MU send function. */
    if (false == forcedWrite)
    {
        /* This is a blocking call */
#if defined(FSL_FEATURE_MU_SIDE_A)
        MU_SendMsg(MU0_MUA, MCMGR_MU_CHANNEL, remoteData);
#elif defined(FSL_FEATURE_MU_SIDE_B)
        MU_SendMsg(MU0_MUB, MCMGR_MU_CHANNEL, remoteData);
#endif
    }
    else
    {
        /* This is a non-blocking call */
#if defined(FSL_FEATURE_MU_SIDE_A)
        MU_SendMsgNonBlocking(MU0_MUA, MCMGR_MU_CHANNEL, remoteData);
#elif defined(FSL_FEATURE_MU_SIDE_B)
        MU_SendMsgNonBlocking(MU0_MUB, MCMGR_MU_CHANNEL, remoteData);
#endif
    }
    return kStatus_MCMGR_Success;
}

/*!
 * @brief ISR handler
 *
 * This function is called when data from MU is received
 */
void mcmgr_mu_channel_handler(void)
{
    uint32_t data;
    uint16_t eventType;
    uint16_t eventData;

    /* Non-blocking version of the receive function needs to be called here to avoid
       deadlock in ISR. The RX register must contain the payload now because the RX flag/event
       has been identified before reaching this point (mcmgr_mu_channel_handler function). */
#if defined(FSL_FEATURE_MU_SIDE_A)
    data = MU_ReceiveMsgNonBlocking(MU0_MUA, MCMGR_MU_CHANNEL);
#elif defined(FSL_FEATURE_MU_SIDE_B)
    data = MU_ReceiveMsgNonBlocking(MU0_MUB, MCMGR_MU_CHANNEL);
#endif

    /* To be MISRA compliant, return value needs to be checked even it could not never be 0 */
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
                    coreNum, eventData, MCMGR_eventTable[(mcmgr_event_type_t)eventType].callbackData);
            }
        }
    }
}

#if defined(MCMGR_HANDLE_EXCEPTIONS) && (MCMGR_HANDLE_EXCEPTIONS == 1)
/* This overrides the weak DefaultISR implementation from startup file */
void DefaultISR(void)
{
    uint32_t exceptionNumber = __get_IPSR();
    (void)MCMGR_TriggerEvent(kMCMGR_RemoteExceptionEvent, (uint16_t)exceptionNumber);
    for (;;)
    {
    } /* stop here */
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
