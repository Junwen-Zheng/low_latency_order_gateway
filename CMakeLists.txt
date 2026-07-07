cmake_minimum_required(VERSION 3.16)

project(low_latency_order_gateway LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(
  -Wall
  -Wextra
  -Wpedantic
  -Wconversion
  -Wshadow
)

include_directories(include)

add_library(llgw_core
  src/exchange_simulator.cpp
  src/feed_replay.cpp
  src/market_data_parser.cpp
  src/order_gateway.cpp
  src/simple_strategy.cpp
)

target_include_directories(llgw_core PUBLIC include)

add_executable(llgw
  src/main.cpp
)

target_link_libraries(llgw PRIVATE llgw_core)

add_executable(test_market_data_parser
  tests/test_market_data_parser.cpp
)

target_link_libraries(test_market_data_parser PRIVATE llgw_core)

add_executable(test_feed_replay
  tests/test_feed_replay.cpp
)

target_link_libraries(test_feed_replay PRIVATE llgw_core)

add_executable(test_exchange_simulator
  tests/test_exchange_simulator.cpp
)

target_link_libraries(test_exchange_simulator PRIVATE llgw_core)

add_executable(test_order_gateway
  tests/test_order_gateway.cpp
)

target_link_libraries(test_order_gateway PRIVATE llgw_core)

add_executable(test_simple_strategy
  tests/test_simple_strategy.cpp
)

target_link_libraries(test_simple_strategy PRIVATE llgw_core)

add_executable(test_end_to_end_order_flow
  tests/test_end_to_end_order_flow.cpp
)

target_link_libraries(test_end_to_end_order_flow PRIVATE llgw_core)


add_executable(test_ring_buffer
  tests/test_ring_buffer.cpp
)

target_link_libraries(test_ring_buffer PRIVATE llgw_core)


add_executable(test_order_pipeline
  tests/test_order_pipeline.cpp
)

target_link_libraries(test_order_pipeline PRIVATE llgw_core)


add_executable(test_end_to_end_pipeline
  tests/test_end_to_end_pipeline.cpp
)

target_link_libraries(test_end_to_end_pipeline PRIVATE llgw_core)


add_executable(test_fixed_symbol
  tests/test_fixed_symbol.cpp
)

target_link_libraries(test_fixed_symbol PRIVATE llgw_core)
