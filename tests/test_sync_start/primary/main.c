/*
 * Copyright 2016-2025 NXP
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

/*******************************************************************************
 * Definitions
 ******************************************************************************/
// This macros are defined for testing mcmgr API functions

// number of cores in the system, this value should return API function
// MCMGR_GetCoreCount()
#define CORE_COUNT 2

// invalid number of core, used as invalid argument to API functions
#define INVALID_CORE_NUMBER 100

// address of RAM, which is used to tests for read/write from both cores
// this address should be from memory region defined by previous macros
#if defined(MIMXRT1176_cm7_SERIES) || defined(MIMXRT1166_cm7_SERIES)
/* Because the RAM the secondary core runs from is not defined as non-cachable
   in rt1170/rt1160 platform, we have to use the start of shared memory instead for
   this platform. */
#define TEST_ADDRESS (((uint32_t)0x202c0000))
#elif (defined(MIMXRT1187_cm33_SERIES) || defined(MIMXRT1189_cm33_SERIES))
#define TEST_ADDRESS (((uint32_t)0x20520000))
#elif defined(KW45B41Z83_cm33_SERIES)
#define TEST_ADDRESS (((uint32_t)0x489C0000))
#elif (defined(KW47B42ZB7_cm33_core0_SERIES) || defined(MCXW727C_cm33_core0_SERIES))
#define TEST_ADDRESS (((uint32_t)0x489C8800))
#elif (defined(MIMXRT798S_cm33_core0_SERIES))
#define TEST_ADDRESS (((uint32_t)0x20200000))
#elif defined(K32L3A60_cm4_SERIES)
#define TEST_ADDRESS (((uint32_t)0x2002E800))
#else
#define TEST_ADDRESS (((uint32_t)CORE1_BOOT_ADDRESS) + 8 * 1024)
#endif
#define TEST_VALUE     0xAAAAAAAA
#define TEST_VALUE_16B 0xBBBB

#if (defined(MIMXRT1187_cm33_SERIES) || defined(MIMXRT1189_cm33_SERIES))
#define EXCEPTION_NUMBER (4)
#else
#define EXCEPTION_NUMBER (3)
#endif
#define HARDFAULT_TRIGGER_EVENT (5)

// RAM address from which boot secondary core
#define BOOT_ADDRESS (void *)(char *)CORE1_BOOT_ADDRESS

/*******************************************************************************
 * Prototypes
 ******************************************************************************/

/*******************************************************************************
 * Code
 ******************************************************************************/
void setUp(void)
{
}

void tearDown(void)
{
}

/* This function is used for the Corn test automation framework
   to breakpoint/stop the execution and to capture results
   from the memory. It must be ensured that it will never be inlined
   and optimized to allow proper address recognition and breakpoint
   placement during the Corn execution. */
__attribute__((noinline)) void CornBreakpointFunc(void)
{
    volatile int i = 0;
    i++;
}

static volatile int exceptionEvent   = 0;
static volatile int appEvent         = 0;
static volatile int remoteCoreUpDone = 0;
static int dummy                     = 0;

void MCMGR_RemoteExceptionEventHandler(mcmgr_core_t coreNum, uint16_t remoteData, void *context)
{
    int *exceptionEvent = (int *)context;
    *exceptionEvent     = 1;

    TEST_ASSERT(remoteData == EXCEPTION_NUMBER);
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

    /* Install remote exception event handler */
    MCMGR_RegisterEvent(kMCMGR_RemoteExceptionEvent, MCMGR_RemoteExceptionEventHandler, (void *)&exceptionEvent);
    // Did MCMGR_RegisterEvent function succeed?
    TEST_ASSERT(retVal == kStatus_MCMGR_Success);

#if defined(KW45B41Z83_cm33_SERIES)
    retVal = MCMGR_StartCore(kMCMGR_Core1, BOOT_ADDRESS, 0xb0000000, kMCMGR_Start_Synchronous);
#elif (defined(KW47B42ZB7_cm33_core0_SERIES) || defined(MCXW727C_cm33_core0_SERIES))
    retVal = MCMGR_StartCore(kMCMGR_Core1, BOOT_ADDRESS, 0xb0008800, kMCMGR_Start_Synchronous);
#else
    retVal = MCMGR_StartCore(kMCMGR_Core1, BOOT_ADDRESS, TEST_ADDRESS, kMCMGR_Start_Synchronous);
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

    /* Trigger kMCMGR_RemoteRPMsgEvent that has no registerred callback on the remote side */
    retVal = MCMGR_TriggerEvent(kMCMGR_Core1, kMCMGR_RemoteRPMsgEvent, 0);
    // Did MCMGR_TriggerEvent function succeed?
    TEST_ASSERT(retVal == kStatus_MCMGR_Success);
}

void mcmgr_test_bad_args()
{
    mcmgr_status_t retVal = kStatus_MCMGR_Error;
    /* Bad event number */
    retVal = MCMGR_TriggerEvent(kMCMGR_Core1, (mcmgr_event_type_t)(kMCMGR_EventTableLength + 1), 123);
    // Did function succeed?
    TEST_ASSERT(retVal == kStatus_MCMGR_Error);

    /* Bad event number using MCMGR_TriggerEventForce() */
    retVal = MCMGR_TriggerEventForce(kMCMGR_Core1, (mcmgr_event_type_t)(kMCMGR_EventTableLength + 1), 123);
    // Did function succeed?
    TEST_ASSERT(retVal == kStatus_MCMGR_Error);

#if (defined(KW45B41Z83_cm33_SERIES) || defined(KW47B42ZB7_cm33_core0_SERIES) || defined(MCXW727C_cm33_core0_SERIES))
    /* Force IMU_SendMsgsBlocking() to return IMU_ERR_TX_FIFO_LOCKED error and consequently the
     * MCMGR_TriggerEventForce() error */
    // IMU_LOCK_TX_FIFO(kIMU_LinkCpu1Cpu2);
    IMU_SendMsgsBlocking(kIMU_LinkCpu1Cpu2, &dummy, 1,
                         true); /* lockSendFifo param needs to be set to true to casue the TX lock */
    retVal = MCMGR_TriggerEventForce(kMCMGR_Core1, kMCMGR_RemoteApplicationEvent, 123);
    // Did function succeed?
    TEST_ASSERT(retVal == kStatus_MCMGR_Error);
    IMU_UNLOCK_TX_FIFO(kIMU_LinkCpu1Cpu2);
#endif

    /* Bad event number */
    retVal = MCMGR_RegisterEvent((mcmgr_event_type_t)(kMCMGR_EventTableLength + 1), NULL, NULL);
    // Did function succeed?
    TEST_ASSERT(retVal == kStatus_MCMGR_Error);

    /* NULL function - disable event */
    retVal = MCMGR_RegisterEvent(kMCMGR_RemoteApplicationEvent, NULL, &dummy);
    // Did function succeed?
    TEST_ASSERT(retVal == kStatus_MCMGR_Success);

    /* NULL data */
    retVal = MCMGR_RegisterEvent(kMCMGR_RemoteApplicationEvent, MCMGR_RemoteApplicationEventHandler, NULL);
    // Did MCMGR_RegisterEvent function succeed?
    TEST_ASSERT(retVal == kStatus_MCMGR_Success);

    retVal = MCMGR_GetCoreProperty(kMCMGR_Core_Num, kMCMGR_CoreStatus, NULL, NULL);
    // Did function succeed?
    TEST_ASSERT(retVal == kStatus_MCMGR_Error);

    retVal = MCMGR_GetStartupData(kMCMGR_Core1, NULL);
    // Did function succeed?
    TEST_ASSERT(retVal == kStatus_MCMGR_Error);
}

void run_tests(void *unused)
{
#ifdef __COVERAGESCANNER__
    __coveragescanner_testname("mcmgr_sync_start_test");
    __coveragescanner_install("mcmgr_sync_start_test.csexe");
#endif /*__COVERAGESCANNER__*/
    RUN_EXAMPLE(mcmgr_test_init_success, MAKE_UNITY_NUM(k_unity_mcmgr, 0));
    RUN_EXAMPLE(mcmgr_test_start_register_trigger, MAKE_UNITY_NUM(k_unity_mcmgr, 5));
    RUN_EXAMPLE(mcmgr_test_bad_args, MAKE_UNITY_NUM(k_unity_mcmgr, 6));
}

int main(void)
{
    BOARD_InitHardware();

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
#ifdef __COVERAGESCANNER__
#if defined(SQUISHCOCO_RESULT_DATA_SAVE_TO_FILE)
    /* Store the secondary core measurement data (saved temporarily in shared memory) into the file */
    FILE *fptr;
    fptr = fopen("mcmgr_sync_start_test_sec_core.csexe", "w");
    fwrite((const void *)(TEST_ADDRESS + 0x10), sizeof(char), *(uint32_t *)TEST_ADDRESS, fptr);
    fclose(fptr);
#elif defined(SQUISHCOCO_RESULT_DATA_SAVE_TO_CONSOLE)
    /* Printf the secondary core measurement data (saved temporarily in shared memory) into the console */
    char *s_ptr = (char *)(TEST_ADDRESS + 0x10);
    for (int32_t i = 0; i < (*(uint32_t *)TEST_ADDRESS); i++)
        PRINTF("%c", s_ptr[i]);
#endif
#endif /*__COVERAGESCANNER__*/
    /* Trigger the event on the secondary side to start code that causes hardfault */
    MCMGR_TriggerEvent(kMCMGR_Core1, kMCMGR_RemoteApplicationEvent, HARDFAULT_TRIGGER_EVENT);

    CornBreakpointFunc();
    while (1)
        ;
}
