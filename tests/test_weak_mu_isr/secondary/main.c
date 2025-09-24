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
#include "mcmgr_internal_core_api.h"
#include "fsl_mu.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define TEST_VALUE 0xAAAAAAAA
#define TEST_VALUE_16B 0xBBBB

#define TEST_MU_CHANNEL 0
#if (defined(MIMXRT1186_cm7_SERIES) || defined(MIMXRT1187_cm7_SERIES) || defined(MIMXRT1189_cm7_SERIES))
#define TEST_MU_BASE    MU1_MUB
#elif (defined(MIMXRT735S_cm33_core1_SERIES) || defined(MIMXRT758S_cm33_core1_SERIES) || \
       defined(MIMXRT798S_cm33_core1_SERIES))
#define TEST_MU_BASE    MU1_MUB
#elif (defined(KW43B43ZC7_NBU_SERIES))
#define TEST_MU_BASE    MU0_MUB
#else
#define TEST_MU_BASE    MUB
#endif

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

void mcmgr_test_weak_mu_isr()
{
    MU_SendMsg(TEST_MU_BASE, TEST_MU_CHANNEL, TEST_VALUE);
    MU_TriggerInterrupts(TEST_MU_BASE, kMU_GenInt0InterruptTrigger);
}

void run_tests(void *unused)
{
    RUN_EXAMPLE(mcmgr_test_init_success_sec_core, MAKE_UNITY_NUM(k_unity_mcmgr, 0));
    RUN_EXAMPLE(mcmgr_test_weak_mu_isr, MAKE_UNITY_NUM(k_unity_mcmgr, 1));
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
