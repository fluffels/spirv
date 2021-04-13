cmake_minimum_required (VERSION 3.0)
set (CMAKE_CXX_STANDARD 17)

project (spirv)

include_directories(${CMAKE_HOME_DIRECTORY}/src)
include_directories(${CMAKE_HOME_DIRECTORY}/lib)
add_executable (
    main
    src/Main.cpp
)
