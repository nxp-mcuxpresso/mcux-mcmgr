/*
 * Copyright 2025 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "unity.h"
#include "assert.h"
#include "app.h"
#include "mcmgr.h"
#include "fsl_mu.h"
#include "fsl_debug_console.h"
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
#define TEST_ADDRESS ((uint32_t)rpmsg_lite_base)
#define TEST_VALUE     0xAAAAAAAA
#define TEST_VALUE_16B 0xBBBB

#define TEST_MU_CHANNEL 0
#if (defined(MIMXRT1186_cm33_SERIES) || defined(MIMXRT1187_cm33_SERIES) || defined(MIMXRT1189_cm33_SERIES))
#define TEST_MU_BASE    MU1_MUA
#elif (defined(MIMXRT735S_cm33_core0_SERIES) || defined(MIMXRT758S_cm33_core0_SERIES) || \
       defined(MIMXRT798S_cm33_core0_SERIES))
#define TEST_MU_BASE    MU1_MUA
#elif (defined(KW43B43ZC7_SERIES))
#define TEST_MU_BASE    MU0_MUA
#else
#define TEST_MU_BASE    MUA
#endif

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

static volatile int appEvent             = 0;
static volatile int genIntFlagIsrReached = 0;
static volatile int rxFullFlagIsrReached = 0;

void MCMGR_RemoteApplicationEventHandler(mcmgr_core_t coreNum, uint16_t remoteData, void *context)
{
    int *appEvent = (int *)context;
    *appEvent     = 1;

    TEST_ASSERT(remoteData == TEST_VALUE_16B);
}

/* Weak MU ISR functions overloading, see weak stub implementations in mcmgr_mu_internal.c */
void MU_GenInt0FlagISR(MU_Type *base, mcmgr_core_t coreNum)
{
    genIntFlagIsrReached = 1;
}

void MU_Rx0FullFlagISR(MU_Type *base, mcmgr_core_t coreNum)
{
    TEST_ASSERT(TEST_VALUE == MU_ReceiveMsgNonBlocking(TEST_MU_BASE, TEST_MU_CHANNEL));
    rxFullFlagIsrReached = 1;
}

// Test of MCMGR_Init() API function
void mcmgr_test_init_success()
{
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

    /* Trigger back an event */
    retVal = MCMGR_TriggerEvent(kMCMGR_Core1, kMCMGR_RemoteApplicationEvent, TEST_VALUE_16B);
    // Did MCMGR_TriggerEvent function succeed?
    TEST_ASSERT(retVal == kStatus_MCMGR_Success);

}

void mcmgr_test_weak_mu_isr()
{
    /* Enable kMU_GenInt0InterruptEnable and kMU_Rx0FullInterruptEnable MU interrupts  */
    MU_EnableInterrupts(TEST_MU_BASE, (kMU_GenInt0InterruptEnable | kMU_Rx0FullInterruptEnable));

    /* Wait untill both MU_GenInt0FlagISR and MU_Rx0FullFlagISR are reached */
    while((genIntFlagIsrReached == 0) || (rxFullFlagIsrReached == 0))
        ;
}

void run_tests(void *unused)
{
    RUN_EXAMPLE(mcmgr_test_init_success, MAKE_UNITY_NUM(k_unity_mcmgr, 0));
    RUN_EXAMPLE(mcmgr_test_start_register_trigger, MAKE_UNITY_NUM(k_unity_mcmgr, 1));
    RUN_EXAMPLE(mcmgr_test_weak_mu_isr, MAKE_UNITY_NUM(k_unity_mcmgr, 2));
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

    while (1)
        ;
}
