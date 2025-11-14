/*
 * Copyright 2025 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "unity.h"
#include "assert.h"
#include "app.h"
#include "mcmgr.h"
#include <stdio.h>
#include <stdlib.h>

/*******************************************************************************
 * Definitions
 ******************************************************************************/
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
#define TEST_ADDRESS   ((uint32_t)rpmsg_lite_base)
#define TEST_VALUE     0xAAAAAAAA
#define TEST_VALUE_16B 0xBBBB

// RAM address from which boot secondary core
#define BOOT_ADDRESS (void *)(char *)CORE1_BOOT_ADDRESS

#define TEST_HEARTBEAT_EVENT_DATA        (0xBEAE)
#define TEST_HEARTBEAT_TIMEOUT_MS        (5000)  /* 5 seconds timeout */
#define TEST_HEARTBEAT_EXPECTED_INTERVAL (1000)  /* Expected every 1 second */
#define TEST_DURATION_MS                 (30000) /* Run for 30 seconds */

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

void setUp(void)
{
}

void tearDown(void)
{
}

static volatile int appEvent                 = 0;
static volatile bool g_heartbeatReceived     = false;
static volatile uint32_t g_heartbeatCount    = 0;
static volatile uint32_t g_lastHeartbeatTime = 0;
static volatile bool g_secondaryCoreAlive    = false;

/*!
 * @brief Heartbeat event handler
 */
void MCMGR_RemoteApplicationEventHandler(mcmgr_core_t coreNum, uint16_t remoteData, void *context)
{
    if (remoteData == TEST_HEARTBEAT_EVENT_DATA)
    {
        g_heartbeatReceived = true;
        g_heartbeatCount++;
        g_lastHeartbeatTime  = GetCurrentTimeMs();
        g_secondaryCoreAlive = true;
    }
    else
    {
        int *appEvent = (int *)context;
        *appEvent     = 1;

        TEST_ASSERT(remoteData == TEST_VALUE_16B);
    }
}

/*!
 * @brief Secondary core exception handler
 */
void SecondaryExceptionHandler(mcmgr_core_t coreNum, uint16_t eventData, void *context)
{
    (void)context;
    (void)eventData;

    PRINTF("Secondary core %d exception detected!\n", coreNum);
    g_secondaryCoreAlive = false;
}

/*!
 * @brief Check if heartbeat timeout occurred
 */
static bool CheckHeartbeatTimeout(void)
{
    uint32_t currentTime            = GetCurrentTimeMs();
    uint32_t timeSinceLastHeartbeat = currentTime - g_lastHeartbeatTime;

    return (g_secondaryCoreAlive && timeSinceLastHeartbeat > TEST_HEARTBEAT_TIMEOUT_MS);
}

// Test of MCMGR_Init() API function
void mcmgr_test_init_success()
{
    /* Initialize SysTick timer */
    InitSysTick();

    /* Initialize MCMGR - low level multicore management library.
       Call this function as close to the reset entry as possible,
       (into the startup sequence) to allow CoreUp event trigerring. */
    mcmgr_status_t retVal = kStatus_MCMGR_Error;

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

    /* Install remote application event */
    retVal = MCMGR_RegisterEvent(kMCMGR_RemoteApplicationEvent, MCMGR_RemoteApplicationEventHandler, (void *)&appEvent);
    // Did MCMGR_RegisterEvent function succeed?
    TEST_ASSERT(retVal == kStatus_MCMGR_Success);

    retVal = MCMGR_StartCore(kMCMGR_Core1, BOOT_ADDRESS, TEST_ADDRESS, kMCMGR_Start_Synchronous);

    // NO DELAY HERE!
    while (!appEvent)
        ;
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
}

/*!
 * @brief Heartbeat test function
 */
void mcmgr_test_heartbeat()
{
    uint32_t testStartTime;
    uint32_t currentTime;
    bool testPassed = true;

    testStartTime       = GetCurrentTimeMs();
    g_lastHeartbeatTime = testStartTime;

    /* Register RemoteExceptionEvent handler in addition to the RemoteApplicationEvent to catch exceptions on the
     * secondary core */
    TEST_ASSERT(kStatus_MCMGR_Success ==
                MCMGR_RegisterEvent(kMCMGR_RemoteExceptionEvent, SecondaryExceptionHandler, NULL));

    /* Monitor heartbeat for 30 seconds */
    while ((GetCurrentTimeMs() - testStartTime) < TEST_DURATION_MS)
    {
        currentTime = GetCurrentTimeMs();

        /* Check for heartbeat timeout */
        if (CheckHeartbeatTimeout())
        {
            /* Heartbeat timeout detected! Secondary core may be unresponsive. */
            testPassed = false;
            break;
        }

        /* Print status every 5 seconds */
        /* if ((currentTime - testStartTime) % TEST_HEARTBEAT_TIMEOUT_MS == 0)
        {
            PRINTF("Status: Received %lu heartbeats, Secondary core alive: %s\n",
                   g_heartbeatCount, g_secondaryCoreAlive ? "YES" : "NO");
        }*/

        /* Small delay to prevent busy waiting */
        SDK_DelayAtLeastUs(10000U, SDK_DEVICE_MAXIMUM_CPU_CLOCK_FREQUENCY);
    }

    TEST_ASSERT(testPassed == true);
    TEST_ASSERT(g_heartbeatCount >= (TEST_DURATION_MS / TEST_HEARTBEAT_EXPECTED_INTERVAL) - 1);
}
void run_tests(void *unused)
{
    RUN_EXAMPLE(mcmgr_test_init_success, MAKE_UNITY_NUM(k_unity_mcmgr, 0));
    RUN_EXAMPLE(mcmgr_test_start_register_trigger, MAKE_UNITY_NUM(k_unity_mcmgr, 1));
    RUN_EXAMPLE(mcmgr_test_heartbeat, MAKE_UNITY_NUM(k_unity_mcmgr, 2));
}

int main(void)
{
    BOARD_InitHardware();

#ifdef CORE1_IMAGE_COPY_TO_RAM
    /* Calculate size of the image - not required on MCUXpressoIDE. MCUXpressoIDE copies image to RAM during startup
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
