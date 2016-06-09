cmake_minimum_required(VERSION 3.5)
project(uvtest)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gun99")

INCLUDE_DIRECTORIES(
        include/
)


set(SOURCE_FILES main.c)
add_executable(raft ${SOURCE_FILES})