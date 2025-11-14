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

/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define TEST_VALUE     0xAAAAAAAA
#define TEST_VALUE_16B 0xBBBB

#define TEST_HEARTBEAT_EVENT_DATA (0xBEAE)

#define TEST_HEARTBEAT_INTERVAL_MS (1000)  /* Send heartbeat every 1 second */
#define TEST_DURATION_MS           (30000) /* Run for 30 seconds */

/*******************************************************************************
 * Variables
 ******************************************************************************/
static volatile uint32_t g_systickCounter = 0;

/*******************************************************************************
 * Prototypes
 ******************************************************************************/

/*******************************************************************************
 * Code
 ******************************************************************************/
static volatile uint32_t g_heartbeatsSent = 0;
static volatile bool g_testRunning        = true;

/*!
 * @brief SysTick interrupt handler
 */
void SysTick_Handler(void)
{
    g_systickCounter++;
}

/*!
 * @brief Initialize SysTick timer for 1ms tick
 */
static void InitSysTick(void)
{
    /* Configure SysTick to generate interrupt every 1ms */
    if (SysTick_Config(SystemCoreClock / 1000U))
    {
        /* Error: SysTick configuration failed */
        while (1)
            ;
    }
}

/*!
 * @brief Get current time in milliseconds using SysTick
 */
static uint32_t GetCurrentTimeMs(void)
{
    return g_systickCounter;
}

/*!
 * @brief Send heartbeat to primary core
 */
static mcmgr_status_t SendHeartbeat(void)
{
    mcmgr_status_t status;

    status = MCMGR_TriggerEvent(kMCMGR_Core0, kMCMGR_RemoteApplicationEvent, TEST_HEARTBEAT_EVENT_DATA);

    if (status == kStatus_MCMGR_Success)
    {
        g_heartbeatsSent++;
    }
    return status;
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

    /* Initialize SysTick timer */
    InitSysTick();

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

    /* Get the startup data */
    do
    {
        status = MCMGR_GetStartupData(kMCMGR_Core0, &startupData);
    } while (status != kStatus_MCMGR_Success);

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

void mcmgr_test_heartbeat()
{
    mcmgr_status_t status;
    uint32_t testStartTime;
    uint32_t lastHeartbeatTime;
    uint32_t currentTime;

    testStartTime     = GetCurrentTimeMs();
    lastHeartbeatTime = testStartTime;

    /* Send initial heartbeat */
    status = SendHeartbeat();
    TEST_ASSERT(status == kStatus_MCMGR_Success);

    /* Main heartbeat loop */
    while (g_testRunning)
    {
        currentTime = GetCurrentTimeMs();

        /* Check if test duration exceeded */
        if ((currentTime - testStartTime) >= TEST_DURATION_MS)
        {
            /* Test duration completed, stopping heartbeat transmission */
            g_testRunning = false;
            break;
        }

        /* Send heartbeat if interval elapsed */
        if ((currentTime - lastHeartbeatTime) >= TEST_HEARTBEAT_INTERVAL_MS)
        {
            status = SendHeartbeat();
            if (status != kStatus_MCMGR_Success)
            {
                /* Heartbeat transmission failed, stopping test */
                break;
            }
            lastHeartbeatTime = currentTime;
        }

        /* Some work can be done here */
        __asm("nop");
        __asm("nop");
        __asm("nop");

        /* Small delay to prevent busy waiting */
        //SDK_DelayAtLeastUs(10000U, SDK_DEVICE_MAXIMUM_CPU_CLOCK_FREQUENCY);
    }

    /* Test completed successfully if we reach here */
    TEST_ASSERT(g_heartbeatsSent >= (TEST_DURATION_MS / TEST_HEARTBEAT_INTERVAL_MS) - 1);
}

void run_tests(void *unused)
{
    RUN_EXAMPLE(mcmgr_test_init_success_sec_core, MAKE_UNITY_NUM(k_unity_mcmgr, 0));
    RUN_EXAMPLE(mcmgr_test_heartbeat, MAKE_UNITY_NUM(k_unity_mcmgr, 1));
}

int main(void)
{
    BOARD_InitHardware();

    UnityBegin();
    run_tests(NULL);
    UnityEnd();

    while (1)
        ;
}
