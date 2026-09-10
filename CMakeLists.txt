cmake_minimum_required(VERSION 3.10)
project(cshell C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Include header directory
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Common core sources
set(CORE_SOURCES
    src/cshell.c
    src/lexer.c
    src/parser.c
    src/builtin.c
    src/executor.c
    src/history.c
)

# Platform-specific implementation
if(WIN32)
    list(APPEND CORE_SOURCES src/platform_win32.c)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
else()
    list(APPEND CORE_SOURCES src/platform_posix.c)
endif()

# Binary output destination
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin)

# Main shell executable
add_executable(cshell src/main.c ${CORE_SOURCES})

# Test runner executable
add_executable(cshell_tests tests/test_runner.c ${CORE_SOURCES})

# Compilation warnings
if(MSVC)
    target_compile_options(cshell PRIVATE /W3)
    target_compile_options(cshell_tests PRIVATE /W3)
else()
    target_compile_options(cshell PRIVATE -Wall -Wextra -pedantic)
    target_compile_options(cshell_tests PRIVATE -Wall -Wextra -pedantic)
endif()
