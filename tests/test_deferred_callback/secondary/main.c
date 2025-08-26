/*
 * Copyright 2025 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "unity.h"
#include "fsl_device_registers.h"
#include "fsl_common.h"
#include "mcmgr.h"
#include "app.h"
#if (defined(KW45B41Z83_cm33_SERIES) || defined(KW47B42ZB7_cm33_core1_SERIES) || defined(MCXW727C_cm33_core1_SERIES))
#include "fsl_imu.h"
#endif
#include "FreeRTOS.h"
#include "task.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define TEST_TASK_STACK_SIZE (600U)
#define TEST_DEFERRED_RX_TASK_STACK_SIZE (250U)

#define TEST_VALUE 0xAAAAAAAA
#define TEST_VALUE_16B 0xBBBB
#define HARDFAULT_TRIGGER_EVENT (5)

/*******************************************************************************
 * Prototypes
 ******************************************************************************/

/*******************************************************************************
 * Code
 ******************************************************************************/
static TaskHandle_t test_task_handle = NULL;
static TaskHandle_t test_deferred_rx_task_handle = NULL;
static volatile int appEvent = 0;

void mcmgr_imu_deferred_call(uint32_t param)
{
    /* Notify/wakeup the task_deferred_rx_task, pass the vector parameter as the target task notification value */
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xTaskNotifyFromISR(test_deferred_rx_task_handle, (1UL << (param & 0xFFFFU)), eSetBits, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
void MCMGR_RemoteApplicationEventHandler(mcmgr_core_t coreNum, uint16_t remoteData, void *context)
{
   int* appEvent = (int*)context;
   *appEvent = remoteData;
}

void setUp(void)
{
}

void tearDown(void)
{
}

void mcmgr_test_init_success_sec_core()
{
    uint32_t startupData;
    mcmgr_status_t status = kStatus_MCMGR_Error;

    /* Initialize MCMGR - low level multicore management library.
       Call this function as close to the reset entry as possible,
       (into the startup sequence) to allow CoreUp event trigerring. */
#if (defined(KW45B41Z83_cm33_SERIES) || defined(KW47B42ZB7_cm33_core1_SERIES) || defined(MCXW727C_cm33_core1_SERIES))
    status = MCMGR_EarlyInit();
    TEST_ASSERT(status == kStatus_MCMGR_Success);
#endif

   /* Initialize MCMGR before calling its API */
    status = MCMGR_Init();
    TEST_ASSERT(status == kStatus_MCMGR_Success);

    /* Install remote application event */
    status = MCMGR_RegisterEvent(kMCMGR_RemoteApplicationEvent, MCMGR_RemoteApplicationEventHandler, (void*)&appEvent);
    TEST_ASSERT(status == kStatus_MCMGR_Success);

    /* Get the startup data */
    do{
        status = MCMGR_GetStartupData(kMCMGR_Core0, &startupData);
    }while(status != kStatus_MCMGR_Success);


    // wait here, to make sure it is not a coincidence
    // the master core manages to read the TEST_VALUE.
    int i;
    for (i = 0; i < 10000; i++)
        ;


    // write the test value
    ((uint32_t *)startupData)[0] = TEST_VALUE;

    // Now, start function on master can return
    // pass the TEST_VALUE_16B to the other side in the callback
    status = MCMGR_TriggerEvent(kMCMGR_Core0, kMCMGR_RemoteApplicationEvent, TEST_VALUE_16B);
    TEST_ASSERT(status == kStatus_MCMGR_Success);
}

void run_tests(void *unused)
{
    RUN_EXAMPLE(mcmgr_test_init_success_sec_core, MAKE_UNITY_NUM(k_unity_mcmgr, 0));
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
            IMU_ClearPendingInterrupts(kIMU_LinkCpu2Cpu1, IMU_MSG_FIFO_CNTL_MSG_RDY_INT_CLR_MASK);
            MCMGR_ProcessDeferredRxIsr();
            NVIC_EnableIRQ((IRQn_Type)CPU2_MSG_RDY_INT_IRQn);
        }
    }
}

static void test_task(void *param)
{

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
