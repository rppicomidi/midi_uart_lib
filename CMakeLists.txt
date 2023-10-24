cmake_minimum_required(VERSION 3.13)

add_library(midi_uart_lib INTERFACE)
target_sources(midi_uart_lib INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/midi_uart_lib.c
)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../ring_buffer_lib lib/ring_buffer_lib)
target_include_directories(midi_uart_lib INTERFACE ${CMAKE_CURRENT_LIST_DIR})
