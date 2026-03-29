# Test programs CMake configuration

# Platform-specific link libraries
if(WIN32)
    set(PLATFORM_LIBS Threads::Threads ws2_32)
else()
    set(PLATFORM_LIBS Threads::Threads)
endif()

# Test server (simulates host PC)
add_executable(test_server test_server.c
    ../src/network/tcp_client.c
    ../src/utils/timestamp.c
    ../src/utils/logger.c
    ../src/utils/config.c
    ../src/utils/adc_error.c
)
target_include_directories(test_server PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(test_server ${PLATFORM_LIBS})

# Ring buffer test
add_executable(test_ring_buffer test_ring_buffer.c
    ../src/buffer/ring_buffer.c
    ../src/utils/timestamp.c
    ../src/utils/logger.c
    ../src/utils/config.c
    ../src/utils/adc_error.c
)
target_include_directories(test_ring_buffer PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(test_ring_buffer ${PLATFORM_LIBS})

# ADC simulator test
add_executable(test_adc_simulator test_adc_simulator.c
    ../src/adc/adc_simulator.c
    ../src/utils/timestamp.c
    ../src/utils/logger.c
    ../src/utils/config.c
    ../src/utils/adc_error.c
)
target_include_directories(test_adc_simulator PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)
if(WIN32)
    target_link_libraries(test_adc_simulator ${PLATFORM_LIBS} m)
else()
    target_link_libraries(test_adc_simulator ${PLATFORM_LIBS} m)
endif()

# Performance test
add_executable(test_performance test_performance.c
    ../src/buffer/ring_buffer.c
    ../src/utils/timestamp.c
    ../src/utils/logger.c
    ../src/utils/config.c
    ../src/utils/adc_error.c
)
target_include_directories(test_performance PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(test_performance ${PLATFORM_LIBS})

# Add tests
add_test(NAME ring_buffer COMMAND test_ring_buffer)
add_test(NAME adc_simulator COMMAND test_adc_simulator)
add_test(NAME performance COMMAND test_performance)