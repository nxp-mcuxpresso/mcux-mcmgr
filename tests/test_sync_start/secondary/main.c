/*
 * Copyright 2016-2025 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "unity.h"
#include "fsl_device_registers.h"
#include "fsl_common.h"
//#include "board.h"
#include "mcmgr.h"
#include "app.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define TEST_VALUE 0xAAAAAAAA
#define TEST_VALUE_16B 0xBBBB
#define HARDFAULT_TRIGGER_EVENT (5)

/*******************************************************************************
 * Prototypes
 ******************************************************************************/

/*******************************************************************************
 * Code
 ******************************************************************************/
static volatile int appEvent = 0;
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

void mcmgr_test_bad_args_sec_core()
{
    mcmgr_status_t retVal = kStatus_MCMGR_Success;
    /* Testing bad args, to be added into unity */
    retVal = MCMGR_GetStartupData(kMCMGR_Core0, NULL);
    TEST_ASSERT(retVal == kStatus_MCMGR_Error);
}

void run_tests(void *unused)
{
#ifdef __COVERAGESCANNER__
    __coveragescanner_testname("mcmgr_sync_start_test_sec_core");
    __coveragescanner_install("mcmgr_sync_start_test_sec_core.csexe");
#endif /*__COVERAGESCANNER__*/
    RUN_EXAMPLE(mcmgr_test_init_success_sec_core, MAKE_UNITY_NUM(k_unity_mcmgr, 0));
    RUN_EXAMPLE(mcmgr_test_bad_args_sec_core, MAKE_UNITY_NUM(k_unity_mcmgr, 1));
}

int main(void)
{
    int i = 0;

    BOARD_InitHardware();

    UnityBegin();
    run_tests(NULL);
    UnityEnd();

    // wait until the primary core notifies hardfault can be triggered
    while(HARDFAULT_TRIGGER_EVENT != appEvent);

    // Now, trigger an exception here! Try to write to flash.
    *((uint32_t*)0xFFFFFFFF) = i;

    while (1)
        ;
}
