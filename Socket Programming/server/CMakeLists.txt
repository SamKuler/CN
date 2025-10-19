cmake_minimum_required(VERSION 3.28)
project(FTPServer C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_compile_options(-Wall -Wextra)

# Option to enable code coverage (gcc/clang). When ON, builds with coverage flags.
option(ENABLE_COVERAGE "Enable coverage build (gcc/clang)" OFF)
if(ENABLE_COVERAGE)
    message(STATUS "Building with coverage flags")
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        add_compile_options(-g -O0 -fprofile-arcs -ftest-coverage)
        link_libraries(--coverage)
    else()
        message(WARNING "Coverage enabled but compiler is not GCC/Clang; coverage flags not set")
    endif()
endif()

# Include header files.
include_directories(${PROJECT_SOURCE_DIR}/include)

set(SOURCES
    src/utils.c
    src/logger.c
    src/filesys.c
    src/network.c
)

# Create library: use shared library when coverage enabled to ensure coverage data is emitted
if(ENABLE_COVERAGE)
    add_library(ftpserver SHARED ${SOURCES})
else()
    add_library(ftpserver STATIC ${SOURCES})
endif()

# Main program
add_executable(server src/main.c)
target_link_libraries(server ftpserver)

enable_testing()

# Add test subdirectory.
add_subdirectory(test)