cmake_minimum_required(VERSION 3.20)
project(Nerith VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Bibliotheque : le coeur de l'emulateur (pour l'instant, un CPU pedagogique)
add_library(nerith_core src/cpu.cpp)
target_include_directories(nerith_core PUBLIC include)
if(MSVC)
    target_compile_options(nerith_core PUBLIC /W4)
else()
    target_compile_options(nerith_core PUBLIC -Wall -Wextra)
endif()

# Programme de demonstration
add_executable(nerith src/main.cpp)
target_link_libraries(nerith PRIVATE nerith_core)

# Tests automatises (lances avec : ctest)
enable_testing()
add_executable(test_cpu tests/test_cpu.cpp)
target_link_libraries(test_cpu PRIVATE nerith_core)
add_test(NAME cpu COMMAND test_cpu)
