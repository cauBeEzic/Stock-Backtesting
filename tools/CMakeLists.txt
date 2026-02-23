add_executable(regenerate_goldens
  regenerate_goldens.cpp
)

target_link_libraries(regenerate_goldens PRIVATE core)
