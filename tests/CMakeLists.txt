add_executable(core_tests
  core_tests.cpp
)

target_link_libraries(core_tests PRIVATE core)
target_compile_definitions(core_tests PRIVATE STOCKBT_SOURCE_DIR="${CMAKE_SOURCE_DIR}")

add_test(NAME core_tests COMMAND core_tests)
