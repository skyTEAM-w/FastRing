# Test programs CMake configuration

# Test server (simulates host PC)
add_executable(test_server test_server.c
    ../src/network/tcp_client.c
    ../src/utils/timestamp.c
    ../src/utils/logger.c
)
target_include_directories(test_server PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(test_server
    Threads::Threads
    ws2_32
)

# Ring buffer test
add_executable(test_ring_buffer test_ring_buffer.c
    ../src/buffer/ring_buffer.c
    ../src/utils/timestamp.c
    ../src/utils/logger.c
)
target_include_directories(test_ring_buffer PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(test_ring_buffer
    Threads::Threads
    ws2_32
)

# ADC simulator test
add_executable(test_adc_simulator test_adc_simulator.c
    ../src/adc/adc_simulator.c
    ../src/utils/timestamp.c
    ../src/utils/logger.c
)
target_include_directories(test_adc_simulator PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(test_adc_simulator
    Threads::Threads
    ws2_32
    m
)

# Performance test
add_executable(test_performance test_performance.c
    ../src/buffer/ring_buffer.c
    ../src/utils/timestamp.c
    ../src/utils/logger.c
)
target_include_directories(test_performance PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(test_performance
    Threads::Threads
    ws2_32
)

# Add tests
add_test(NAME ring_buffer COMMAND test_ring_buffer)
add_test(NAME adc_simulator COMMAND test_adc_simulator)
add_test(NAME performance COMMAND test_performance)