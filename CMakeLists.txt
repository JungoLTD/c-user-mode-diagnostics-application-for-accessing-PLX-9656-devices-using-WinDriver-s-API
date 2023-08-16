cmake_minimum_required(VERSION 3.0)

project(p9656_diag C)
include(../../../../include/wd.cmake)
include_directories(
    ../../shared
    ../../../../include
    )

set(SRCS
    p9656_diag.c
    )

add_executable(p9656_diag ${SRCS} ../diag_lib/plx_diag_lib.c
    ../lib/plx_lib.c ${SAMPLE_SHARED_SRCS})
target_compile_definitions(p9656_diag PRIVATE P9656_LITE_BOARD)
target_link_libraries(p9656_diag ${WDAPI_LIB})
if(${ARCH} STREQUAL "LINUX")
    target_link_libraries(p9656_diag m)
endif()
set_target_properties(p9656_diag PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${ARCH}/")
