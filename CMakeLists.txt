#
# Copyright 2024-2025 NXP
#
# SPDX-License-Identifier: BSD-3-Clause
#

if (CONFIG_MCUX_COMPONENT_middleware.multicore.mcmgr)
    mcux_add_include(
        INCLUDES src
    )

    mcux_add_source(
        SOURCES src/mcmgr.h
                  src/mcmgr_internal_core_api.h
                  src/mcmgr.c
    )

    mcux_add_configuration(
            CC "-DMULTICORE_APP=1"
    )
endif()

if(CONFIG_MCUX_COMPONENT_middleware.multicore.mcmgr.imxrt700)
    mcux_add_source(
            SOURCES src/mcmgr_internal_core_api_imxrt700.c
                    src/mcmgr_mu_internal.c
    )
endif()

if(CONFIG_MCUX_COMPONENT_middleware.multicore.mcmgr.imxrt1160)
    mcux_add_source(
            SOURCES src/mcmgr_internal_core_api_imxrt1160.c
                    src/mcmgr_mu_internal.c
    )
endif()

if (CONFIG_MCUX_COMPONENT_middleware.multicore.mcmgr.imxrt1170)
    mcux_add_source(
            SOURCES src/mcmgr_internal_core_api_imxrt1170.c
                    src/mcmgr_mu_internal.c
    )
endif()

if(CONFIG_MCUX_COMPONENT_middleware.multicore.mcmgr.imxrt1180)
    mcux_add_source(
            SOURCES src/mcmgr_internal_core_api_imxrt1180.c
                    src/mcmgr_mu_internal.c
    )
endif()

if(CONFIG_MCUX_COMPONENT_middleware.multicore.mcmgr.k32l3a6)
    mcux_add_source(
            SOURCES src/mcmgr_internal_core_api_k32l3a6.c
                    src/mcmgr_mu_internal.c
    )
endif()

if(CONFIG_MCUX_COMPONENT_middleware.multicore.mcmgr.k32w1)
    mcux_add_source(
            SOURCES src/mcmgr_internal_core_api_k32w1.c
                    src/mcmgr_internal_core_api_k32w1_ext.c
                    src/mcmgr_imu_internal.c
                    src/mcmgr_imu_internal.h
    )
endif()

if(CONFIG_MCUX_COMPONENT_middleware.multicore.mcmgr.kw45b41)
    mcux_add_source(
            SOURCES src/mcmgr_internal_core_api_kw45b41.c
                    src/mcmgr_internal_core_api_kw45b41_ext.c
                    src/mcmgr_imu_internal.c
                    src/mcmgr_imu_internal.h
    )
endif()

if(CONFIG_MCUX_COMPONENT_middleware.multicore.mcmgr.kw47b42)
    mcux_add_source(
            SOURCES src/mcmgr_internal_core_api_kw47b42.c
                    src/mcmgr_internal_core_api_kw47b42_ext.c
                    src/mcmgr_imu_internal.c
                    src/mcmgr_imu_internal.h
    )
endif()

if(CONFIG_MCUX_COMPONENT_middleware.multicore.mcmgr.kw43b43)
    mcux_add_source(
            SOURCES src/mcmgr_internal_core_api_kw43b43.c
                    src/mcmgr_mu_internal.c
    )
endif()

if(CONFIG_MCUX_COMPONENT_middleware.multicore.mcmgr.lpc55s69)
    mcux_add_source(
            SOURCES src/mcmgr_internal_core_api_lpc55s69.c
    )
endif()

if(CONFIG_MCUX_COMPONENT_middleware.multicore.mcmgr.lpc54114)
    mcux_add_source(
            SOURCES src/mcmgr_internal_core_api_lpc54114.c
    )
endif()

if(CONFIG_MCUX_COMPONENT_middleware.multicore.mcmgr.mcxnx4x)
    mcux_add_source(
            SOURCES src/mcmgr_internal_core_api_mcxnx4x.c
    )
endif()

if(CONFIG_MCUX_COMPONENT_middleware.multicore.mcmgr.mcxl20)
    mcux_add_source(
            SOURCES src/mcmgr_internal_core_api_mcxl20.c
                    src/mcmgr_mu_internal.c
            )
endif()

if(CONFIG_MCUX_COMPONENT_middleware.multicore.mcmgr.mcxw716)
    mcux_add_source(
            SOURCES src/mcmgr_internal_core_api_mcxw716.c
                    src/mcmgr_internal_core_api_mcxw716_ext.c
                    src/mcmgr_imu_internal.c
                    src/mcmgr_imu_internal.h
    )
endif()

if(CONFIG_MCUX_COMPONENT_middleware.multicore.mcmgr.mcxw727)
    mcux_add_source(
            SOURCES src/mcmgr_internal_core_api_mcxw727.c
                    src/mcmgr_internal_core_api_mcxw727_ext.c
                    src/mcmgr_imu_internal.c
                    src/mcmgr_imu_internal.h
    )
endif()
