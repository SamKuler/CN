cmake_minimum_required(VERSION 3.28)
project(FTPServer C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_compile_options(-Wall -Wextra)

# Include header files.
include_directories(${PROJECT_SOURCE_DIR}/include)

set(SOURCES
    src/utils.c
    src/logger.c
    src/filesys.c
)

# Create static library
add_library(ftpserver STATIC ${SOURCES})

# Main program
add_executable(server src/main.c)
target_link_libraries(server ftpserver)

enable_testing()

# Add test subdirectory.
add_subdirectory(test)