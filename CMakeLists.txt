cmake_minimum_required(VERSION 3.30)
project(spsc_fifo C)

set(CMAKE_C_STANDARD 11)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic")

add_executable(spsc example/spsc.c example/spsc-fifo.c)
