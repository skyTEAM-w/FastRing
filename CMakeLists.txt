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

# Shared compile settings
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(adc_acquisition PRIVATE -O3 -Wall -Wextra)
    if(CMAKE_GENERATOR MATCHES "Ninja")
        target_compile_options(adc_acquisition PRIVATE -fdiagnostics-color=always)
    endif()
elseif(MSVC)
    target_compile_options(adc_acquisition PRIVATE /O2 /W4 /MP)
endif()

# Platform-specific libraries
if(WIN32)
    target_link_libraries(adc_acquisition
        Threads::Threads
        ws2_32
    )
else()
    target_link_libraries(adc_acquisition
        Threads::Threads
        m
    )
endif()

# Test programs
enable_testing()
add_subdirectory(tests)

# Install rules
install(TARGETS adc_acquisition
    RUNTIME DESTINATION bin
)
