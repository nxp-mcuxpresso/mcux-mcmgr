/*
 * Copyright 2025 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "unity.h"
#include "assert.h"
#include "app.h"
#include "mcmgr.h"
#include "fsl_debug_console.h"
#include <stdio.h>
#include <stdlib.h>
#if (defined(KW45B41Z83_cm33_SERIES) || defined(KW47B42ZB7_cm33_core0_SERIES) || defined(MCXW727C_cm33_core0_SERIES))
#include "fsl_imu.h"
#endif
#include "FreeRTOS.h"
#include "task.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define TEST_TASK_STACK_SIZE (600U)
#define TEST_DEFERRED_RX_TASK_STACK_SIZE (250U)

#define SH_MEM_TOTAL_SIZE (6144)
#if defined(__ICCARM__) /* IAR Workbench */
#pragma location = "rpmsg_sh_mem_section"
char rpmsg_lite_base[SH_MEM_TOTAL_SIZE];
#elif defined(__CC_ARM) || defined(__ARMCC_VERSION) /* Keil MDK */
char rpmsg_lite_base[SH_MEM_TOTAL_SIZE] __attribute__((section("rpmsg_sh_mem_section")));
#elif defined(__GNUC__)
char rpmsg_lite_base[SH_MEM_TOTAL_SIZE] __attribute__((section(".noinit.$rpmsg_sh_mem")));
#endif

// address of RAM, which is used to tests for read/write from both cores
#if defined(KW45B41Z83_cm33_SERIES)
#define TEST_ADDRESS (((uint32_t)0x489C0000))
#else
#define TEST_ADDRESS ((uint32_t)rpmsg_lite_base)
#endif

#define TEST_VALUE     0xAAAAAAAA
#define TEST_VALUE_16B 0xBBBB

// RAM address from which boot secondary core
#define BOOT_ADDRESS (void *)(char *)CORE1_BOOT_ADDRESS

/*******************************************************************************
 * Prototypes
 ******************************************************************************/

/*******************************************************************************
 * Code
 ******************************************************************************/
static TaskHandle_t test_task_handle = NULL;
static TaskHandle_t test_deferred_rx_task_handle = NULL;
void setUp(void)
{
}

void tearDown(void)
{
}

static volatile int appEvent         = 0;
static volatile int remoteCoreUpDone = 0;
static volatile int deferredCallEvent = 0;

void mcmgr_imu_deferred_call(uint32_t param)
{
    /* Notify/wakeup the task_deferred_rx_task, pass the vector parameter as the target task notification value */
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xTaskNotifyFromISR(test_deferred_rx_task_handle, (1UL << (param & 0xFFFFU)), eSetBits, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void MCMGR_RemoteApplicationEventHandler(mcmgr_core_t coreNum, uint16_t remoteData, void *context)
{
    int *appEvent = (int *)context;
    *appEvent     = 1;

    TEST_ASSERT(remoteData == TEST_VALUE_16B);
}

void MCMGR_RemoteCoreUpEventHandler(mcmgr_core_t coreNum, uint16_t remoteData, void *context)
{
    int *remoteCoreUpDone = (int *)context;
    *remoteCoreUpDone     = 1;
}

// Test of MCMGR_Init() API function
void mcmgr_test_init_success()
{
    /* Initialize MCMGR - low level multicore management library.
       Call this function as close to the reset entry as possible,
       (into the startup sequence) to allow CoreUp event trigerring. */
    mcmgr_status_t retVal = kStatus_MCMGR_Error;
#if (defined(KW45B41Z83_cm33_SERIES) || defined(KW47B42ZB7_cm33_core0_SERIES) || defined(MCXW727C_cm33_core0_SERIES))
    retVal = MCMGR_EarlyInit();
    TEST_ASSERT(retVal == kStatus_MCMGR_Success);
#endif

    retVal = MCMGR_Init();
    TEST_ASSERT(retVal == kStatus_MCMGR_Success);
}

void mcmgr_test_start_register_trigger()
{
    mcmgr_status_t retVal = kStatus_MCMGR_Error;

    // set 4bytes at TEST_ADDRESS to zero
    uint32_t *addr = (uint32_t *)TEST_ADDRESS;
    addr[0]        = 0x0;

#ifdef APP_INVALIDATE_CACHE_FOR_SECONDARY_CORE_IMAGE_MEMORY
    invalidate_cache_for_core1_image_memory(TEST_ADDRESS, 4);
#endif /* APP_INVALIDATE_CACHE_FOR_SECONDARY_CORE_IMAGE_MEMORY */

    /* Install remote reset event handler */
    retVal = MCMGR_RegisterEvent(kMCMGR_RemoteCoreUpEvent, MCMGR_RemoteCoreUpEventHandler, (void *)&remoteCoreUpDone);
    // Did MCMGR_RegisterEvent function succeed?
    TEST_ASSERT(retVal == kStatus_MCMGR_Success);

    /* Install remote application event */
    retVal = MCMGR_RegisterEvent(kMCMGR_RemoteApplicationEvent, MCMGR_RemoteApplicationEventHandler, (void *)&appEvent);
    // Did MCMGR_RegisterEvent function succeed?
    TEST_ASSERT(retVal == kStatus_MCMGR_Success);

#if defined(KW45B41Z83_cm33_SERIES)
    retVal = MCMGR_StartCore(kMCMGR_Core1, BOOT_ADDRESS, 0xb0000000, kMCMGR_Start_Asynchronous);
#elif (defined(KW47B42ZB7_cm33_core0_SERIES) || defined(MCXW727C_cm33_core0_SERIES))
    retVal = MCMGR_StartCore(kMCMGR_Core1, BOOT_ADDRESS, 0xb0008800, kMCMGR_Start_Asynchronous);
#else
    retVal = MCMGR_StartCore(kMCMGR_Core1, BOOT_ADDRESS, TEST_ADDRESS, kMCMGR_Start_Asynchronous);
#endif
    // NO DELAY HERE!
    while (!appEvent)
        ;
    // remoteCoreUp event must be received at the time application event is received...
    TEST_ASSERT(remoteCoreUpDone == 1);

#if (defined(MIMXRT1187_cm33_SERIES) || defined(MIMXRT1189_cm33_SERIES))
    // wait here a little to avoid armgcc release target fail when reading the value from the shared mem.
    volatile int i;
    for (i = 0; i < 10000; i++)
        ;
#endif

#ifdef APP_INVALIDATE_CACHE_FOR_SECONDARY_CORE_IMAGE_MEMORY
    invalidate_cache_for_core1_image_memory(TEST_ADDRESS, 4);
#endif /* APP_INVALIDATE_CACHE_FOR_SECONDARY_CORE_IMAGE_MEMORY */

    // read uint32 from TEST_ADDRESS
    uint32_t val = addr[0];

    // Did start function succeed?
    TEST_ASSERT(retVal == kStatus_MCMGR_Success);

    // compare with value TEST_VALUE, which should write secondary code after start
    TEST_ASSERT(val == TEST_VALUE);

    /* Trigger back an event */
    retVal = MCMGR_TriggerEvent(kMCMGR_Core1, kMCMGR_RemoteApplicationEvent, TEST_VALUE_16B);
    // Did MCMGR_TriggerEvent function succeed?
    TEST_ASSERT(retVal == kStatus_MCMGR_Success);

}

/*!
 * test_deferred_rx_task
 *
 * Deffered rx task used for rx data processing outside the interrupt context.
 * During the inter-cpu isr only the necessary steps are done like clearing respective interrupt flags and notifying 
 * the waiting deferred rx task. Once the isr finishes the deferred rx task is scheduled and it handles the data 
 * processing. In case of IMU, clearing interrupt flags is not done in isr but in deferred task.
 *
 */
static void test_deferred_rx_task(void * pvParameters)
{
    uint32_t ulBits = 0UL;
    while (1)
    {
        /* Wait for the notification from the isr */ 
        if(pdPASS == xTaskNotifyWait( pdFALSE, 0xffffffffU, &ulBits, portMAX_DELAY ))
        {
            IMU_ClearPendingInterrupts(kIMU_LinkCpu1Cpu2, IMU_MSG_FIFO_CNTL_MSG_RDY_INT_CLR_MASK);
            MCMGR_ProcessDeferredRxIsr();
            NVIC_EnableIRQ((IRQn_Type)RF_IMU0_IRQn);
        }
    }
}

void run_tests(void *unused)
{
    RUN_EXAMPLE(mcmgr_test_init_success, MAKE_UNITY_NUM(k_unity_mcmgr, 0));
    RUN_EXAMPLE(mcmgr_test_start_register_trigger, MAKE_UNITY_NUM(k_unity_mcmgr, 5));
}

static void test_task(void *param)
{
#ifdef CORE1_IMAGE_COPY_TO_RAM
    /* Calculate size of the image - not required on LPCExpresso. LPCExpresso copies image to RAM during startup
     * automatically */
    uint32_t core1_image_size;
    core1_image_size = get_core1_image_size();
    PRINTF("Copy Secondary core image to address: 0x%x, size: %d\r\n", CORE1_BOOT_ADDRESS, core1_image_size);

    /* Copy Secondary core application from FLASH to RAM. Primary core code is executed from FLASH, Secondary from RAM
     * for maximal effectivity.*/
    memcpy((void *)(char *)CORE1_BOOT_ADDRESS, (void *)CORE1_IMAGE_START, core1_image_size);

#ifdef APP_INVALIDATE_CACHE_FOR_SECONDARY_CORE_IMAGE_MEMORY
    invalidate_cache_for_core1_image_memory(CORE1_BOOT_ADDRESS, core1_image_size);
#endif /* APP_INVALIDATE_CACHE_FOR_SECONDARY_CORE_IMAGE_MEMORY */

#endif /* CORE1_IMAGE_COPY_TO_RAM */

    UnityBegin();
    run_tests(NULL);
    UnityEnd();

    while (1)
        ;
}

int main(void)
{
    BOARD_InitHardware();

    if (xTaskCreate(test_task, "TEST_TASK", TEST_TASK_STACK_SIZE, NULL, tskIDLE_PRIORITY + 1U, &test_task_handle) != pdPASS)
    {
        for (;;)
        {
        }
    }
    if (xTaskCreate(test_deferred_rx_task, "TEST_DEFERRED_RX_TASK", TEST_DEFERRED_RX_TASK_STACK_SIZE, NULL, tskIDLE_PRIORITY + 2U, &test_deferred_rx_task_handle) != pdPASS)
    {
        for (;;)
        {
        }
    }

    vTaskStartScheduler();
    return 0;
}
