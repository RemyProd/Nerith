cmake_minimum_required(VERSION 3.20)
project(Nerith VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Reglages communs a tous les modules : dossier des en-tetes et avertissements au maximum
function(nerith_module name)
    target_include_directories(${name} PUBLIC ${PROJECT_SOURCE_DIR}/include)
    if(MSVC)
        target_compile_options(${name} PUBLIC /W4)
    else()
        target_compile_options(${name} PUBLIC -Wall -Wextra)
    endif()
endfunction()

# --- Modules (bibliotheques) ---
# Dependances : toy -> memory ; ppc -> memory ; util et memory ne dependent de rien.

# util : outils generaux (fichiers, logs)
add_library(nerith_util
    src/util/file.cpp
    src/util/log.cpp
)
nerith_module(nerith_util)

# memory : la memoire de l'emulateur (big-endian, acces verifies)
add_library(nerith_memory
    src/memory/memory.cpp
)
nerith_module(nerith_memory)

# toy : le CPU pedagogique, son assembleur et son desassembleur
add_library(nerith_toy
    src/toy/cpu.cpp
    src/toy/assembler.cpp
    src/toy/disassembler.cpp
)
nerith_module(nerith_toy)
target_link_libraries(nerith_toy PUBLIC nerith_memory)

# ppc : le CPU de la Xbox 360 (PowerPC) ; pour l'instant, le decodeur et le desassembleur
add_library(nerith_ppc
    src/ppc/decoder.cpp
    src/ppc/disassembler.cpp
)
nerith_module(nerith_ppc)
target_link_libraries(nerith_ppc PUBLIC nerith_memory)

# --- Programme principal ---
add_executable(nerith src/main.cpp)
target_link_libraries(nerith PRIVATE nerith_toy nerith_util)

# --- Tests automatises (lances avec : ctest) ---
enable_testing()

function(nerith_test name library)
    add_executable(test_${name} tests/test_${name}.cpp)
    target_link_libraries(test_${name} PRIVATE ${library})
    add_test(NAME ${name} COMMAND test_${name})
endfunction()

nerith_test(memory       nerith_memory)
nerith_test(file         nerith_util)
nerith_test(log          nerith_util)
nerith_test(cpu          nerith_toy)
nerith_test(assembler    nerith_toy)
nerith_test(disassembler nerith_toy)
nerith_test(ppc_decoder  nerith_ppc)
nerith_test(ppc_disassembler nerith_ppc)
