cmake_minimum_required(VERSION 3.27)
set (CMAKE_CXX_COMPILER /usr/bin/g++-14)
project(my-project LANGUAGES CXX VERSION 0.0.1)
set (CMAKE_CXX_STANDARD 23)
set (CMAKE_CXX_STANDARD_REQUIRED ON)
set (CMAKE_CXX_EXTENSIONS OFF)
set (CMAKE_EXPORT_COMPILE_COMMANDS ON)
Include(FetchContent)
# add all executables
add_executable(montecarlo src/montecarlo.cpp )
add_library(my_headers0 INTERFACE)
target_include_directories(my_headers0 INTERFACE include)
target_link_libraries(montecarlo PRIVATE my_headers0)
# begin dependencies from cxxdeps.txt
# cxxdeps dependency Catch2
FetchContent_Declare(Catch2 GIT_REPOSITORY https://github.com/catchorg/Catch2.git GIT_TAG v3.3.1)
FetchContent_MakeAvailable(Catch2)
# finally, add all sources
set(SOURCES
)
target_sources(montecarlo PRIVATE ${SOURCES})