cmake_minimum_required(VERSION 3.10)

project(md2cs
  VERSION 0.2
  DESCRIPTION "Transform a markdown script into a Coding Stories"
  LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_subdirectory(include)
add_subdirectory(src)

target_include_directories(md2cs PUBLIC
  "${PROJECT_BINARY_DIR}/include"
  "${PROJECT_SOURCE_DIR}/include"
  )
