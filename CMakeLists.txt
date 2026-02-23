cmake_minimum_required(VERSION 3.20)
project(StockBacktesting VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_APP "Build Qt desktop app" ON)

add_subdirectory(core)
add_subdirectory(tests)
add_subdirectory(tools)

if(BUILD_APP)
  find_package(Qt6 COMPONENTS Widgets Concurrent Charts QUIET)
  if(Qt6_FOUND)
    add_subdirectory(app)
  else()
    message(WARNING "Qt6 not found; skipping app target. Configure with -DBUILD_APP=OFF to silence this warning.")
  endif()
endif()
