cmake_minimum_required(VERSION 3.13)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION FALSE)
set(CMAKE_C_FLAGS_RELEASE "-Os")
add_compile_definitions(
        NDEBUG

# boot rp2040 at 48MHz, later overclocked in main()
        SYS_CLK_KHZ=48000
        XOSC_KHZ=54000

        PLL_COMMON_REFDIV=1

        PLL_SYS_VCO_FREQ_KHZ=864000
        PLL_SYS_POSTDIV1=6
        PLL_SYS_POSTDIV2=3

        PLL_USB_VCO_FREQ_KHZ=864000
        PLL_USB_POSTDIV1=6
        PLL_USB_POSTDIV2=3
)
add_link_options(-Wl,--print-memory-usage)

# Pull in SDK (must be before project)
set(SKIP_HARDWARE_XOSC 1)
include(pico_sdk_import.cmake)

project(picoave C CXX ASM)
set(CMAKE_C_STANDARD 11)

pico_sdk_init()

unset(SKIP_HARDWARE_XOSC)
pico_add_subdirectory(hardware_xosc)

add_executable(picoave
        src/main.c
        src/packets.c
        src/packet_info_frame.c
        src/video_output.c
        src/tmds_clock.c
        src/tmds_serializer.c
        src/tmds_encode.c
        src/gpu_input.c
        src/mem_pool.c
        src/font.c
        src/str_builder.c
)

pico_generate_pio_header(picoave ${CMAKE_CURRENT_LIST_DIR}/src/pio/clock.pio)
pico_generate_pio_header(picoave ${CMAKE_CURRENT_LIST_DIR}/src/pio/serializer.pio)
pico_generate_pio_header(picoave ${CMAKE_CURRENT_LIST_DIR}/src/pio/capture.pio)
pico_generate_pio_header(picoave ${CMAKE_CURRENT_LIST_DIR}/src/pio/i2s.pio)

execute_process(
        COMMAND git log -1 --format=%h
        WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
        OUTPUT_VARIABLE GIT_COMMIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
)

target_compile_definitions(picoave PRIVATE
        PICO_STACK_SIZE=512
        GIT_COMMIT_HASH="${GIT_COMMIT_HASH}"
)

target_compile_options(picoave PUBLIC
        ### Gnu/Clang C Options
        $<$<COMPILE_LANG_AND_ID:C,GNU>:-fdiagnostics-color=always>
        $<$<COMPILE_LANG_AND_ID:C,Clang>:-fcolor-diagnostics>

        $<$<COMPILE_LANG_AND_ID:C,Clang,GNU>:-Wall>
        $<$<COMPILE_LANG_AND_ID:C,Clang,GNU>:-Wextra>
        $<$<COMPILE_LANG_AND_ID:C,Clang>:-Weverything>
)

target_include_directories(picoave PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/../../../../Common/include
)

target_link_libraries(picoave
        pico_multicore
        pico_stdlib
        hardware_dma
        hardware_irq
        hardware_pio
        hardware_interp
)

pico_add_extra_outputs(picoave)

pico_enable_stdio_usb(picoave 0)
pico_enable_stdio_uart(picoave 0)

pico_set_binary_type(picoave default)
