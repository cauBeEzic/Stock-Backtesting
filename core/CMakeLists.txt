add_library(core STATIC
  src/csv_importer.cpp
  src/backtester.cpp
  src/exporter.cpp
  src/downsampling.cpp
  src/time_utils.cpp
)

target_include_directories(core
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(core PUBLIC cxx_std_17)

if(MSVC)
  target_compile_options(core PRIVATE /W4 /permissive-)
else()
  target_compile_options(core PRIVATE -Wall -Wextra -Wpedantic)
endif()
