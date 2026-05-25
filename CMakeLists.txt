cmake_minimum_required(VERSION 3.20)
project(LifeEngineCpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(lifeengine_core src/core.cpp)
target_include_directories(lifeengine_core PUBLIC include)
target_compile_options(lifeengine_core PRIVATE -Wall -Wextra -pedantic)

add_library(lifeengine_gui_model src/gui/application.cpp)
target_link_libraries(lifeengine_gui_model PUBLIC lifeengine_core)
target_include_directories(lifeengine_gui_model PUBLIC include)
target_compile_options(lifeengine_gui_model PRIVATE -Wall -Wextra -pedantic)

add_executable(lifeengine src/main.cpp)
target_link_libraries(lifeengine PRIVATE lifeengine_core)

add_executable(lifeengine_gui src/gui/main.cpp src/gui/win32_app.cpp)
target_link_libraries(lifeengine_gui PRIVATE lifeengine_gui_model)
if(WIN32)
    target_link_libraries(lifeengine_gui PRIVATE comctl32 gdi32 user32)
endif()

add_executable(lifeengine_tests tests/core_tests.cpp)
target_link_libraries(lifeengine_tests PRIVATE lifeengine_core)

add_executable(lifeengine_gui_model_tests tests/gui_model_tests.cpp)
target_link_libraries(lifeengine_gui_model_tests PRIVATE lifeengine_gui_model)

enable_testing()
add_test(NAME lifeengine_core_tests COMMAND lifeengine_tests)
add_test(NAME lifeengine_gui_model_tests COMMAND lifeengine_gui_model_tests)
