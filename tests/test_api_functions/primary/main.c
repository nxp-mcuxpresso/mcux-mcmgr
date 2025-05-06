/*
 * Copyright 2016-2025 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <string.h>
#include "board.h"
#include "mcmgr.h"
#include "unity.h"
#include <stdio.h>
#include "app.h"
#include "fsl_debug_console.h"

// Libraries necessary to work with FW ELE, defined by previous macros.
#if (defined(MIMXRT1187_cm33_SERIES) || defined(MIMXRT1189_cm33_SERIES))
#include "ele_crypto.h" /* ELE Crypto SW */
#include "fsl_s3mu.h"   /* Messaging unit driver */
#endif

/*******************************************************************************
 * Definitions
 ******************************************************************************/
// This macros are defined for testing mcmgr API functions

// number of cores in the system, this value should return API function
// MCMGR_GetCoreCount()
#if defined(MIMXRT798S_cm33_core0_SERIES)
#define CORE_COUNT 4
#else
#define CORE_COUNT 2
#endif

// invalid number of core, used as invalid argument to API functions
#define INVALID_CORE_NUMBER 100

// address of RAM, which is used to tests for read/write from both cores
// this address should be from memory region defined by previous macros
#if defined(KW45B41Z83_cm33_SERIES)
#define TEST_ADDRESS (((uint32_t)0x489C0000))
#elif (defined(KW47B42ZB7_cm33_core0_SERIES) || defined(MCXW727C_cm33_core0_SERIES))
#define TEST_ADDRESS (((uint32_t)0x489C8800))
#elif defined(K32L3A60_cm4_SERIES)
#define TEST_ADDRESS (((uint32_t)0x2002E800))
#elif (defined(MIMXRT1187_cm33_SERIES) || defined(MIMXRT1189_cm33_SERIES))
#define S3MU         MU_RT_S3MUA
#define TEST_ADDRESS (((uint32_t)0x20520000))
#elif (defined(MIMXRT798S_cm33_core0_SERIES))
#define TEST_ADDRESS            (((uint32_t)0x20200000))
#define CPU1_RAM_ATBITTER0_ADDR (0x40041044)
#else
#define TEST_ADDRESS (((uint32_t)CORE1_BOOT_ADDRESS) + 0x100)
#endif
#define TEST_VALUE 0xAAAAAAAA

// RAM address from which boot secondary core
#define BOOT_ADDRESS (void *)(char *)CORE1_BOOT_ADDRESS

// value which should return API function MCMGR_GetCurrentCore() for this core
#define THIS_CORE kMCMGR_Core0

/*******************************************************************************
 * Prototypes
 ******************************************************************************/

/*******************************************************************************
 * Code
 ******************************************************************************/
#if (defined(KW45B41Z83_cm33_SERIES) || defined(KW47B42ZB7_cm33_core0_SERIES) || defined(MCXW727C_cm33_core0_SERIES) || defined(K32L3A60_cm4_SERIES))
/* secondary core code runs always from flash, put the minimalistic image into the secondary core flash */
#if defined(__ICCARM__) /* IAR Workbench */
#pragma location = "__core1_image"
__root const uint32_t code[]
#elif defined(__GNUC__)
const uint32_t code[] __attribute__((section(".core1_code")))
#endif
    = {
#if defined(KW45B41Z83_cm33_SERIES)
        (uint32_t)0x0014C000, // SP
        (uint32_t)0x9,        // PC
        0x49024801,           // first two instructions
        0xe7fe6001,           // next two instructions
        0xB0000000,           // test address where second core writes (TEST_ADDRESS & 0x0000FFFFu) + 0xB0000000u)
        TEST_VALUE            // value second core writes
#elif (defined(KW47B42ZB7_cm33_core0_SERIES) || defined(MCXW727C_cm33_core0_SERIES))
        (uint32_t)0x00154000, // SP
        (uint32_t)0x9,        // PC
        0x49024801,           // first two instructions
        0xe7fe6001,           // next two instructions
        0xB0008800,           // test address where second core writes (TEST_ADDRESS & 0x0000FFFFu) + 0xB0000000u)
        TEST_VALUE            // value second core writes
#elif defined(K32L3A60_cm4_SERIES)
        (uint32_t)0x09020000, // SP
        (uint32_t)0x9,        // PC
        0x49034802,           // first two instructions
        0xe7fe6001,           // next two instructions
        0xbf00bf00,           // Filler of NOPs to keep aligment
        TEST_ADDRESS,         // test address where second core writes
        TEST_VALUE            // value second core writes
#endif

 };
#endif

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

// Test MCMGR_StartCore() function with wrong core number, this core is now running
void mcmgr_test_start_core1()
{
    mcmgr_status_t retVal = MCMGR_StartCore(THIS_CORE, BOOT_ADDRESS, 0, kMCMGR_Start_Asynchronous);
    TEST_ASSERT(retVal == kStatus_MCMGR_Error);
}

// Test MCMGR_StartCore() function with invalid core number
void mcmgr_test_start_core2()
{
    mcmgr_status_t retVal =
        MCMGR_StartCore((mcmgr_core_t)INVALID_CORE_NUMBER, BOOT_ADDRESS, 0, kMCMGR_Start_Asynchronous);
    TEST_ASSERT(retVal == kStatus_MCMGR_Error);
}

// Test MCMGR_StartCore function with correct arguments
// In this test is at TEST_ADDRESS written zero value (uint32), next is copied code
// for secondary core to BOOT_ADDRESS and secondary core is started by API function.
// Secondary core should write TEST_VALUE at TEST_ADDRESS and stop in infinite loop,
// main core wait 100ms for start and executing code of secondary core and reads
// value from TEST_ADDRESS and test if value is equal to TEST_VALUE
void mcmgr_test_start_core3()
{
    // set 4bytes at TEST_ADDRESS to zero
    volatile uint32_t *addr = (uint32_t *)TEST_ADDRESS;
    *addr                   = 0x0;

#ifdef APP_INVALIDATE_CACHE_FOR_SECONDARY_CORE_IMAGE_MEMORY
    invalidate_cache_for_core1_image_memory((uint32_t)TEST_ADDRESS, 4);
#endif /* APP_INVALIDATE_CACHE_FOR_SECONDARY_CORE_IMAGE_MEMORY */

#if !(defined(KW45B41Z83_cm33_SERIES) || defined(KW47B42ZB7_cm33_core0_SERIES) || defined(MCXW727C_cm33_core0_SERIES) || defined(K32L3A60_cm4_SERIES))
    // code for secondary core
    // writes TEST_VALUE at TEST_ADDRESS, stops in infinite loop
    //    SP
    //    PC
    // ********* Only For RT700 ***********
    //    MOVS R1 #1         0x2101 - Copy number 1 into R1
    //    LDR R0, [PC, #12]  0x4803 - Copy address to write Arbitter0 to R0
    //    STR R1, [R0]       0x6001 - Write R1 value to R0 address set above
    //    NOP                0xbf00 - NOP instruction to align code to 4 bytes
    // ************************************
    //    LDR R0, [PC, #4]   0x4802
    //    LDR R1, [PC, #8]   0x4903
    //    STR R1, [R0]       0x6001
    //    B .                0xe7fe
    //    NOP                0xbf00

    uint32_t code[] = {
/* RT1180 CM7 core runs from ITCM with aliased address, SP and PC need to be extra defined */
#if (defined(MIMXRT1187_cm33_SERIES) || defined(MIMXRT1189_cm33_SERIES))
        (uint32_t)0x20500000, // SP
        (uint32_t)0x9,        // PC
/* MCXL20 CM0+ core runs from RAM with aliased address, SP and PC need to be extra defined */
#elif (defined(MCXL255_cm33_SERIES))
        (uint32_t)0x00005000, // SP
        (uint32_t)0x9,        // PC
#else
        (uint32_t)TEST_ADDRESS + 0x100, // SP
        (uint32_t)BOOT_ADDRESS + 0x9,   // PC
#endif
#if (defined(MIMXRT798S_cm33_core0_SERIES))
        0x48032101,
        0xbf006001,
#endif
        0x49034802, // first two instructions
        0xe7fe6001, // next two instructions
#if (defined(MIMXRT798S_cm33_core0_SERIES))
        CPU1_RAM_ATBITTER0_ADDR,
#else
        0xbf00bf00,                      // Filler of NOPs to keep aligment
#endif
        TEST_ADDRESS, // test address where second core writes
        TEST_VALUE    // value second core writes
    };

    // copy code for secondary core to RAM starting at BOOT_ADDRESS
    void *secCoreBootAddr = BOOT_ADDRESS;
    memcpy(secCoreBootAddr, code, sizeof(code));

#ifdef APP_INVALIDATE_CACHE_FOR_SECONDARY_CORE_IMAGE_MEMORY
    invalidate_cache_for_core1_image_memory((uint32_t)BOOT_ADDRESS, 0x200);
#endif /* APP_INVALIDATE_CACHE_FOR_SECONDARY_CORE_IMAGE_MEMORY */

#endif

    // start the second core
    mcmgr_status_t retVal = MCMGR_StartCore(kMCMGR_Core1, BOOT_ADDRESS, 0, kMCMGR_Start_Asynchronous);
    TEST_ASSERT(retVal == kStatus_MCMGR_Success);

    // wait for start and executing code of secondary core
    volatile int i;
    for (i = 0; i < 10000; i++)
        ;

#ifdef APP_INVALIDATE_CACHE_FOR_SECONDARY_CORE_IMAGE_MEMORY
    invalidate_cache_for_core1_image_memory((uint32_t)TEST_ADDRESS, 4);
#endif /* APP_INVALIDATE_CACHE_FOR_SECONDARY_CORE_IMAGE_MEMORY */

    // read uint32 from TEST_ADDRESS
    volatile uint32_t val = *addr;

    // compare with value TEST_VALUE, which should write secondary code after start
    TEST_ASSERT(val == TEST_VALUE);
}

// Test ELE_GetFwStatus() function check if FW ELE is loaded
#if (defined(MIMXRT1187_cm33_SERIES) || defined(MIMXRT1189_cm33_SERIES))
void mcmgr_test_get_ele_stat()
{
    uint32_t fwstatus = 0xFFFFFFFFu;
    TEST_ASSERT(ELE_GetFwStatus(S3MU, &fwstatus) == kStatus_Success);
    TEST_ASSERT(fwstatus == 0u);
}
#endif

// Test MCMGR_StopCore() function with wrong core number, this core is now running
void mcmgr_test_stop_core1()
{
    mcmgr_status_t retVal = MCMGR_StopCore(THIS_CORE);
    TEST_ASSERT(retVal == kStatus_MCMGR_Error);
}

// Test MCMGR_StopCore() function with invalid core number
void mcmgr_test_stop_core2()
{
    mcmgr_status_t retVal = MCMGR_StopCore((mcmgr_core_t)INVALID_CORE_NUMBER);
    TEST_ASSERT(retVal == kStatus_MCMGR_Error);
}

// Test MCMGR_StopCore() function with valid core number
// The test address is cleared and if the second core is
// really stopped, it should remain cleared.
void mcmgr_test_stop_core3()
{
#if defined(MIMXRT1176_cm7_SERIES) || defined(MIMXRT1166_cm7_SERIES) || defined(MCXL255_cm33_SERIES)
    mcmgr_status_t retVal = MCMGR_StopCore(kMCMGR_Core1);
    TEST_ASSERT(retVal == kStatus_MCMGR_NotImplemented);
#else
    mcmgr_status_t retVal = MCMGR_StopCore(kMCMGR_Core1);
    TEST_ASSERT(retVal == kStatus_MCMGR_Success);

#if !(defined(KW45B41Z83_cm33_SERIES) || defined(KW47B42ZB7_cm33_core0_SERIES) || defined(MCXW727C_cm33_core0_SERIES))
    // set 4bytes at TEST_ADDRESS to zero
    uint32_t *addr = (uint32_t *)TEST_ADDRESS;
    *addr          = 0x0;

    // wait for start and executing code of secondary core
    int i;
    for (i = 0; i < 10000; i++)
        ;

    // read uint32 from TEST_ADDRESS
    uint32_t val = *addr;

    // compare with value 0 - the second core should be stopped.
    TEST_ASSERT(val == 0);
#endif
#endif
}

// Test if return value of MCMGR_GetVersion() API function is equal to MCMGR
// version enum (kMCMGR_Version)
void mcmgr_test_get_version()
{
    int32_t mcmgrVersion = MCMGR_GetVersion();
    TEST_ASSERT(mcmgrVersion == kMCMGR_Version);
}

// Test if return value of MCMGR_MapAddress() API function is equal to CORE_COUNT
void mcmgr_test_get_core_count()
{
    uint32_t numCores = MCMGR_GetCoreCount();
    TEST_ASSERT(numCores == CORE_COUNT);
}

// Test if return value of MCMGR_GetCurrentCore() API function is equal to
// value defined for this core (THIS_CORE)
void mcmgr_test_get_current_core()
{
    mcmgr_core_t currentCore = MCMGR_GetCurrentCore();
    TEST_ASSERT(currentCore == THIS_CORE);
}

// Test of MCMGR_GetCoreProperty() API function, test if set correct value size
// of property kMCMGR_CoreStatus
void mcmgr_test_get_core_property1()
{
    uint32_t val          = 0;
    uint32_t len          = sizeof(val);
    mcmgr_status_t retVal = MCMGR_GetCoreProperty(kMCMGR_Core0, kMCMGR_CoreStatus, &val, &len);
    if (retVal != kStatus_MCMGR_NotImplemented)
    {
        TEST_ASSERT(retVal == kStatus_MCMGR_Success);
        TEST_ASSERT(val == kMCMGR_CoreTypeCortexM0Plus);
    };
}

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

int main(int argc, char **argv)
{
    BOARD_InitHardware();

    PRINTF("\r\n**** MCMGR TESTS ****\r\n");

    UnityBegin();
#ifdef __COVERAGESCANNER__
    __coveragescanner_testname("mcmgr_test");
    __coveragescanner_install("mcmgr_test.csexe");
#endif /*__COVERAGESCANNER__*/
    RUN_EXAMPLE(mcmgr_test_init_success, MAKE_UNITY_NUM(k_unity_mcmgr, 0));

    RUN_EXAMPLE(mcmgr_test_start_core1, MAKE_UNITY_NUM(k_unity_mcmgr, 5));
    RUN_EXAMPLE(mcmgr_test_start_core2, MAKE_UNITY_NUM(k_unity_mcmgr, 6));
    RUN_EXAMPLE(mcmgr_test_start_core3, MAKE_UNITY_NUM(k_unity_mcmgr, 7));

#if (defined(MIMXRT1187_cm33_SERIES) || defined(MIMXRT1189_cm33_SERIES))
    RUN_EXAMPLE(mcmgr_test_get_ele_stat, MAKE_UNITY_NUM(k_unity_mcmgr, 8));
#endif /* (defined(defined(MIMXRT1187_cm33_SERIES) ||  defined(MIMXRT1189_cm33_SERIES)) */

#if !(defined(MIMXRT1176_cm7_SERIES) || defined(MIMXRT1166_cm7_SERIES) || defined(MIMXRT1187_cm33_SERIES) || \
    defined(MIMXRT1189_cm33_SERIES) || defined(MIMXRT798S_cm33_core0_SERIES) || defined(MCXL255_cm33_SERIES))
    RUN_EXAMPLE(mcmgr_test_stop_core1, MAKE_UNITY_NUM(k_unity_mcmgr, 9));
    RUN_EXAMPLE(mcmgr_test_stop_core2, MAKE_UNITY_NUM(k_unity_mcmgr, 10));
#endif /* !(defined(MIMXRT1176_cm7_SERIES) || defined(MIMXRT1166_cm7_SERIES) || defined(MIMXRT1187_cm33_SERIES) || \
          defined(MIMXRT1189_cm33_SERIES) || defined(MIMXRT798S_cm33_core0_SERIES) || defined(MCXL255_cm33_SERIES)) */
#if !(defined(MIMXRT1187_cm33_SERIES) || defined(MIMXRT1189_cm33_SERIES) || defined(MIMXRT798S_cm33_core0_SERIES))
    RUN_EXAMPLE(mcmgr_test_stop_core3, MAKE_UNITY_NUM(k_unity_mcmgr, 11));
#endif /* !(defined(MIMXRT1187_cm33_SERIES) ||  defined(MIMXRT1189_cm33_SERIES) || \
          defined(MIMXRT798S_cm33_core0_SERIES)) */

    RUN_EXAMPLE(mcmgr_test_get_version, MAKE_UNITY_NUM(k_unity_mcmgr, 15));
    RUN_EXAMPLE(mcmgr_test_get_core_count, MAKE_UNITY_NUM(k_unity_mcmgr, 19));
    RUN_EXAMPLE(mcmgr_test_get_current_core, MAKE_UNITY_NUM(k_unity_mcmgr, 20));
    RUN_EXAMPLE(mcmgr_test_get_core_property1, MAKE_UNITY_NUM(k_unity_mcmgr, 21));

    UnityEnd();
    CornBreakpointFunc();

    while (1)
    {
    };
}
