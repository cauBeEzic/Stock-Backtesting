qt_standard_project_setup()

qt_add_executable(app
  src/main.cpp
  src/main_window.cpp
  src/main_window.hpp
)

target_link_libraries(app
  PRIVATE
    Qt6::Widgets
    Qt6::Concurrent
    Qt6::Charts
    core
)

target_compile_features(app PRIVATE cxx_std_17)
