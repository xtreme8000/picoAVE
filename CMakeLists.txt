cmake_minimum_required(VERSION 3.13)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
set(CMAKE_C_FLAGS_RELEASE "-Ofast")

# Pull in SDK (must be before project)
include(pico_sdk_import.cmake)

project(picoave C CXX ASM)
set(CMAKE_C_STANDARD 11)

pico_sdk_init()

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
)

pico_generate_pio_header(picoave ${CMAKE_CURRENT_LIST_DIR}/src/pio/clock.pio)
pico_generate_pio_header(picoave ${CMAKE_CURRENT_LIST_DIR}/src/pio/serializer.pio)
pico_generate_pio_header(picoave ${CMAKE_CURRENT_LIST_DIR}/src/pio/capture.pio)
pico_generate_pio_header(picoave ${CMAKE_CURRENT_LIST_DIR}/src/pio/i2s.pio)

target_compile_definitions(picoave PRIVATE
        PICO_STACK_SIZE=512
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
