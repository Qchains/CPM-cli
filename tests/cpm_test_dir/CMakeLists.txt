cmake_minimum_required(VERSION 3.10)
project( [Õf±û VERSION °@òèÿÿ LANGUAGES C)

# Set C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Compiler flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra")
set(CMAKE_C_FLAGS_DEBUG "-g -O0")
set(CMAKE_C_FLAGS_RELEASE "-O2 -DNDEBUG")

# Include directories
include_directories(include)

# Source files
file(GLOB_RECURSE SOURCES "src/*.c")
file(GLOB_RECURSE HEADERS "include/*.h")

# Create library
add_library( [Õf±û ${SOURCES} ${HEADERS})

# Create example executable (optional)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/examples/main.c")
    add_executable( [Õf±û_example examples/main.c)
    target_link_libraries( [Õf±û_example  [Õf±û)
endif()

# Install targets
install(TARGETS  [Õf±û
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib)
install(DIRECTORY include/ DESTINATION include)
