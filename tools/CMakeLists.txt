add_executable(regenerate_goldens
  regenerate_goldens.cpp
)

target_link_libraries(regenerate_goldens PRIVATE core)

add_executable(benchmark_mvp
  benchmark_mvp.cpp
)

target_link_libraries(benchmark_mvp PRIVATE core)
