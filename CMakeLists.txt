cmake_minimum_required(VERSION 3.10)

# set the project name
project(md2cs
  VERSION 0.1
  DESCRIPTION "Transform a markdown script into a Coding Stories"
  LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_subdirectory(src)
add_subdirectory(include)
