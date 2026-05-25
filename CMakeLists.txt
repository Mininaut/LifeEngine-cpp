cmake_minimum_required(VERSION 3.20)
project(LifeEngineCpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(lifeengine_core src/core.cpp)
target_include_directories(lifeengine_core PUBLIC include)
target_compile_options(lifeengine_core PRIVATE -Wall -Wextra -pedantic)

add_executable(lifeengine src/main.cpp)
target_link_libraries(lifeengine PRIVATE lifeengine_core)

add_executable(lifeengine_tests tests/core_tests.cpp)
target_link_libraries(lifeengine_tests PRIVATE lifeengine_core)

enable_testing()
add_test(NAME lifeengine_core_tests COMMAND lifeengine_tests)
