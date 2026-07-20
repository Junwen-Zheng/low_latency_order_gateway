cmake_minimum_required(VERSION 3.16)

project(low_latency_order_gateway LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(
  LLGW_WARNINGS_AS_ERRORS
  "Treat compiler warnings as errors"
  OFF
)

option(
  LLGW_ENABLE_SANITIZERS
  "Enable platform-supported runtime sanitizers"
  OFF
)

add_compile_options(
  -Wall
  -Wextra
  -Wpedantic
  -Wconversion
  -Wshadow
)

if(LLGW_WARNINGS_AS_ERRORS)
  add_compile_options(-Werror)
endif()

if(LLGW_ENABLE_SANITIZERS)
  if(CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
    # AddressSanitizer in AppleClang 17 can deadlock during dyld startup on
    # newer macOS releases before main() executes. Use UBSan locally on
    # AppleClang; Linux CI still runs ASan and UBSan together.
    set(LLGW_SANITIZER_FLAGS
      -fsanitize=undefined
    )
    message(STATUS "Sanitizers: UndefinedBehaviorSanitizer (AppleClang)")
  elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    set(LLGW_SANITIZER_FLAGS
      -fsanitize=address,undefined
    )
    message(STATUS "Sanitizers: AddressSanitizer and UndefinedBehaviorSanitizer")
  else()
    message(
      FATAL_ERROR
      "LLGW_ENABLE_SANITIZERS requires Clang, AppleClang, or GCC"
    )
  endif()

  add_compile_options(
    ${LLGW_SANITIZER_FLAGS}
    -fno-omit-frame-pointer
  )
  add_link_options(
    ${LLGW_SANITIZER_FLAGS}
    -fno-omit-frame-pointer
  )
endif()

include(CTest)
enable_testing()

add_library(llgw_core
  src/exchange_simulator.cpp
  src/execution_report.cpp
  src/feed_replay.cpp
  src/market_data_parser.cpp
  src/order_gateway.cpp
  src/order_lifecycle.cpp
  src/pre_trade_risk.cpp
  src/simple_strategy.cpp
)

target_include_directories(llgw_core PUBLIC include)

add_executable(llgw
  src/main.cpp
)

target_link_libraries(llgw PRIVATE llgw_core)

function(llgw_add_test target source)
  add_executable(${target} ${source})
  target_link_libraries(${target} PRIVATE llgw_core)
  add_test(NAME ${target} COMMAND ${target})
endfunction()

llgw_add_test(
  test_market_data_parser
  tests/test_market_data_parser.cpp
)
llgw_add_test(
  test_feed_replay
  tests/test_feed_replay.cpp
)
llgw_add_test(
  test_exchange_simulator
  tests/test_exchange_simulator.cpp
)
llgw_add_test(
  test_order_gateway
  tests/test_order_gateway.cpp
)
llgw_add_test(
  test_simple_strategy
  tests/test_simple_strategy.cpp
)
llgw_add_test(
  test_end_to_end_order_flow
  tests/test_end_to_end_order_flow.cpp
)
llgw_add_test(
  test_ring_buffer
  tests/test_ring_buffer.cpp
)
llgw_add_test(
  test_order_pipeline
  tests/test_order_pipeline.cpp
)
llgw_add_test(
  test_end_to_end_pipeline
  tests/test_end_to_end_pipeline.cpp
)
llgw_add_test(
  test_fixed_symbol
  tests/test_fixed_symbol.cpp
)
llgw_add_test(
  test_pre_trade_risk
  tests/test_pre_trade_risk.cpp
)
llgw_add_test(
  test_end_to_end_risk
  tests/test_end_to_end_risk.cpp
)
llgw_add_test(
  test_order_lifecycle
  tests/test_order_lifecycle.cpp
)
llgw_add_test(
  test_end_to_end_lifecycle
  tests/test_end_to_end_lifecycle.cpp
)
llgw_add_test(
  test_order_actions
  tests/test_order_actions.cpp
)
llgw_add_test(
  test_end_to_end_order_actions
  tests/test_end_to_end_order_actions.cpp
)
llgw_add_test(
  test_execution_report
  tests/test_execution_report.cpp
)
llgw_add_test(
  test_end_to_end_execution_report
  tests/test_end_to_end_execution_report.cpp
)
llgw_add_test(
  test_hardening
  tests/test_hardening.cpp
)

add_test(NAME llgw_demo COMMAND llgw)
set_tests_properties(
  llgw_demo
  PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

add_executable(bench_parser
  benchmarks/bench_parser.cpp
)

target_link_libraries(bench_parser PRIVATE llgw_core)

add_executable(bench_end_to_end
  benchmarks/bench_end_to_end.cpp
)

target_link_libraries(bench_end_to_end PRIVATE llgw_core)

add_executable(bench_path_comparison
  benchmarks/bench_path_comparison.cpp
)

target_link_libraries(bench_path_comparison PRIVATE llgw_core)
