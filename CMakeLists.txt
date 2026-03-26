cmake_minimum_required(VERSION 3.16)
project(ADCDataAcquisition VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Ninja build settings
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Set default build type if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

# Ninja-specific settings
if(CMAKE_GENERATOR MATCHES "Ninja")
    # Enable colored output for Ninja
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fdiagnostics-color=always")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fdiagnostics-color=always")
endif()

# Compiler options
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-O3 -Wall -Wextra -pthread)
    add_compile_options(-march=native -mtune=native)
elseif(MSVC)
    add_compile_options(/O2 /W4 /MP)
endif()

# Thread library
find_package(Threads REQUIRED)

# Source files
set(SOURCES
    src/main.c
    src/adc/adc_simulator.c
    src/buffer/ring_buffer.c
    src/thread/thread_manager.c
    src/thread/thread_workers.c
    src/network/tcp_client.c
    src/utils/timestamp.c
    src/utils/logger.c
    src/utils/config.c
    src/utils/adc_error.c
)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

# Main executable
add_executable(adc_acquisition ${SOURCES})
target_link_libraries(adc_acquisition
    Threads::Threads
    ws2_32
)

# Test programs
enable_testing()
add_subdirectory(tests)

# Install rules
install(TARGETS adc_acquisition
    RUNTIME DESTINATION bin
)