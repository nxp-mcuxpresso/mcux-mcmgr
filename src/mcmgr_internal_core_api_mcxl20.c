/*
 * Copyright 2025 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "mcmgr.h"
#include "mcmgr_internal_core_api.h"
#include "fsl_device_registers.h"
#include "fsl_mu.h"

/* At start make decision for what core mcmgr is build for at the compile time */
#if (defined(FSL_FEATURE_MU_SIDE_A))
#define MCMGR_BUILD_FOR_CORE_0
#elif (defined(FSL_FEATURE_MU_SIDE_B))
#define MCMGR_BUILD_FOR_CORE_1
#else
#error "Building for not supported platform!"
#endif

/* MU TR/RR $MCMGR_MU_CHANNEL is managed by MCMGR */
#define MU_RX_ISR_Handler(x)     MU_RX_ISR(x)
#define MU_RX_ISR(number)        MU_Rx##number##FullFlagISR
#define mcmgr_mu_channel_handler MU_RX_ISR_Handler(MCMGR_MU_CHANNEL)
#define MU_RX_ISR_FLAG_Mask(x)   MU_RX_ISR_FLAG(x)
#define MU_RX_ISR_FLAG(number)   kMU_Rx##number##FullInterruptEnable
#define mcmgr_mu_channel_flag    MU_RX_ISR_FLAG_Mask(MCMGR_MU_CHANNEL)

/* MCMGR boot address of RAM */
#define MCMGR_BOOT_ADDRESS_RAM (0xA1000000U)

volatile mcmgr_core_context_t s_mcmgrCoresContext[MCMGR_CORECOUNT] = {
    {.state = kMCMGR_ResetCoreState, .startupData = 0}, {.state = kMCMGR_ResetCoreState, .startupData = 0}};

/* Initialize structure with informations of all cores */
static const mcmgr_core_info_t s_mcmgrCores[MCMGR_CORECOUNT] = {
    {.coreType = kMCMGR_CoreTypeCortexM33, .coreName = "Main"},
    {.coreType = kMCMGR_CoreTypeCortexM0Plus, .coreName = "Secondary"}};

const mcmgr_system_info_t g_mcmgrSystem = {
    .coreCount = MCMGR_CORECOUNT, .memRegCount = MCMGR_MEMREGCOUNT, .cores = s_mcmgrCores};

mcmgr_status_t mcmgr_early_init_internal(mcmgr_core_t coreNum)
{
    /* This function is intended to be called as close to the reset entry as possible,
       (within the startup sequence in SystemInitHook) to allow CoreUp event triggering.
       Avoid using uninitialized data here. */
       mcmgr_core_t target_core;

    switch (coreNum)
    {
        case kMCMGR_Core0:
/* MUA clk enable */
#if defined(MCMGR_BUILD_FOR_CORE_0)
            target_core = kMCMGR_Core1;
            MU_Init(MUA);
            MU_ResetBothSides(MUA);
#endif
            break;
        case kMCMGR_Core1:
#if defined(MCMGR_BUILD_FOR_CORE_1)
            target_core = kMCMGR_Core0;
            MU_Init(MUB);
#endif
            break;
        default:
            return kStatus_MCMGR_Error;
    }

    /* Trigger core up event here, core is starting! */
    return MCMGR_TriggerEvent(target_core, kMCMGR_RemoteCoreUpEvent, 0);
}

mcmgr_status_t mcmgr_late_init_internal(mcmgr_core_t coreNum)
{
#if defined(MCMGR_BUILD_FOR_CORE_0)
    MU_EnableInterrupts(MUA, (uint32_t)mcmgr_mu_channel_flag);

#if (defined(FSL_FEATURE_MU_HAS_RESET_ASSERT_INT) && FSL_FEATURE_MU_HAS_RESET_ASSERT_INT)
    MU_EnableInterrupts(MUA, (uint32_t)kMU_ResetAssertInterruptEnable);
#endif

    NVIC_SetPriority(MU_A_RX_IRQn, 2);

    NVIC_EnableIRQ(MU_A_RX_IRQn);

#elif defined(MCMGR_BUILD_FOR_CORE_1)
    MU_EnableInterrupts(MUB, (uint32_t)mcmgr_mu_channel_flag);

#if (defined(FSL_FEATURE_MU_HAS_RESET_ASSERT_INT) && FSL_FEATURE_MU_HAS_RESET_ASSERT_INT)
    MU_EnableInterrupts(MUB, (uint32_t)kMU_ResetAssertInterruptEnable);
#endif

    NVIC_SetPriority(MU_B_RX_IRQn, 2);

    NVIC_EnableIRQ(MU_B_RX_IRQn);

#endif

    return kStatus_MCMGR_Success;
}

mcmgr_status_t mcmgr_start_core_internal(mcmgr_core_t coreNum, void *bootAddress)
{
    if ((coreNum != kMCMGR_Core1)  || (bootAddress != (void *)(char *)MCMGR_BOOT_ADDRESS_RAM))
    {
        return kStatus_MCMGR_Error;
    }
#if defined(MCMGR_BUILD_FOR_CORE_0)
    AON__CGU->PER_CLK_EN = 0x7fe7;
    AON__CGU->RST_SUB_BLK &= ~CGU_RST_SUB_BLK_CM0P_RST_REL_MASK;
    AON__CGU->RST_SUB_BLK |= CGU_RST_SUB_BLK_CM0P_RST_REL_MASK;
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
#if defined(MCMGR_BUILD_FOR_CORE_0)
    return kMCMGR_Core0;
#elif defined(MCMGR_BUILD_FOR_CORE_1)
    return kMCMGR_Core1;
#endif
}

mcmgr_status_t mcmgr_trigger_event_internal(mcmgr_core_t coreNum, uint32_t remoteData, bool forcedWrite)
{
    /* Can be unused for two core platform. ifdefs are selecting core to trigger. */
    (void)coreNum;

    /* When forcedWrite is false, execute the blocking call, i.e. wait until previously
       sent data is processed. Otherwise, run the non-blocking version of the MU send function. */
    if (false == forcedWrite)
    {
        /* This is a blocking call */
#if defined(MCMGR_BUILD_FOR_CORE_0)
        MU_SendMsg(MUA, MCMGR_MU_CHANNEL, remoteData);
#elif defined(MCMGR_BUILD_FOR_CORE_1)
        MU_SendMsg(MUB, MCMGR_MU_CHANNEL, remoteData);
#endif
    }
    else
    {
        /* This is a non-blocking call */
#if defined(MCMGR_BUILD_FOR_CORE_0)
        MU_SendMsgNonBlocking(MUA, MCMGR_MU_CHANNEL, remoteData);
#elif defined(MCMGR_BUILD_FOR_CORE_1)
        MU_SendMsgNonBlocking(MUB, MCMGR_MU_CHANNEL, remoteData);
#endif
    }
    return kStatus_MCMGR_Success;
}

/*!
 * @brief ISR handler
 *
 * This function is called when data from MU is received
 */
void mcmgr_mu_channel_handler(MU_Type *base, mcmgr_core_t coreNum)
{
    uint32_t data;
    uint16_t eventType;
    uint16_t eventData;

    /* Non-blocking version of the receive function needs to be called here to avoid
       deadlock in ISR. The RX register must contain the payload now because the RX flag/event
       has been identified before reaching this point (mcmgr_mu_channel_handler function). */
    data = MU_ReceiveMsgNonBlocking(base, MCMGR_MU_CHANNEL);

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
    mcmgr_core_t target_core;
    uint32_t exceptionNumber = __get_IPSR();

    /* Select what core to trigger in case of exception */
#if defined(MCMGR_BUILD_FOR_CORE_0)
    target_core = kMCMGR_Core1;
#else
    target_core = kMCMGR_Core0;
#endif

    (void)MCMGR_TriggerEvent(target_core, kMCMGR_RemoteExceptionEvent, (uint16_t)exceptionNumber);
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

#if defined(__CORTEX_M) && (__CORTEX_M == (33U))
/* Cortex-M33 contains additional exception handlers */
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
#endif

#endif /* MCMGR_HANDLE_EXCEPTIONS */
