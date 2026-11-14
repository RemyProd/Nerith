cmake_minimum_required(VERSION 3.20)
project(Nerith VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Bibliotheque : le coeur de l'emulateur (pour l'instant, un CPU pedagogique)
add_library(nerith_core
    src/assembler.cpp
    src/cpu.cpp
    src/memory.cpp
    src/disassembler.cpp
    src/file.cpp
    src/log.cpp
)
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

add_executable(test_memory tests/test_memory.cpp)
target_link_libraries(test_memory PRIVATE nerith_core)
add_test(NAME memory COMMAND test_memory)

add_executable(test_assembler tests/test_assembler.cpp)
target_link_libraries(test_assembler PRIVATE nerith_core)
add_test(NAME assembler COMMAND test_assembler)

add_executable(test_file tests/test_file.cpp)
target_link_libraries(test_file PRIVATE nerith_core)
add_test(NAME file COMMAND test_file)

add_executable(test_log tests/test_log.cpp)
target_link_libraries(test_log PRIVATE nerith_core)
add_test(NAME log COMMAND test_log)

add_executable(test_disassembler tests/test_disassembler.cpp)
target_link_libraries(test_disassembler PRIVATE nerith_core)
add_test(NAME disassembler COMMAND test_disassembler)
