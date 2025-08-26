#
# Copyright 2024-2025 NXP
#
# SPDX-License-Identifier: BSD-3-Clause

include(${SdkRootDirPath}/examples/_boards/${board}/multicore_examples/reconfig.cmake OPTIONAL)
include(${CMAKE_CURRENT_LIST_DIR}/_boards/${board}/${core_id}/reconfig.cmake OPTIONAL)

mcux_add_configuration(
        CC "-DMCMGR_HANDLE_EXCEPTIONS -D__SEMIHOST_HARDFAULT_DISABLE"
)
if(CONFIG_MCUX_COMPONENT_utilities.gcov)
    # Get all source files in the directory
    file(GLOB_RECURSE MCMGR_SOURCES "${SdkRootDirPath}/middleware/multicore/mcmgr/src/*.c")

    # Set properties for all those files
    foreach(SOURCE_FILE ${MCMGR_SOURCES})
        message("GCOV: Adding coverage flags for ${SOURCE_FILE}")
        set_source_files_properties(${SOURCE_FILE} PROPERTIES
            COMPILE_FLAGS "-g3 -ftest-coverage -fprofile-arcs -fkeep-inline-functions -fkeep-static-functions")
    endforeach()
endif()
