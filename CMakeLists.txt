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
  src/feed_replay.cpp
  src/market_data_parser.cpp
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
