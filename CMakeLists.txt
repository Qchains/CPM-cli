cmake_minimum_required(VERSION 3.16)
project(CPM VERSION 0.1.0 LANGUAGES C)

# Set C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Compiler flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Werror")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g -O0 -DDEBUG")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3 -DNDEBUG")

# Find required packages
find_package(PkgConfig REQUIRED)

# Find PMDK
pkg_check_modules(PMDK REQUIRED libpmem libpmemobj)

# Find cJSON
pkg_check_modules(CJSON REQUIRED libcjson)

# Find pthread
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# Include directories
include_directories(include)
include_directories(${PMDK_INCLUDE_DIRS})
include_directories(${CJSON_INCLUDE_DIRS})

# Library directories
link_directories(${PMDK_LIBRARY_DIRS})
link_directories(${CJSON_LIBRARY_DIRS})

# Collect source files
file(GLOB_RECURSE LIB_SOURCES "lib/*.c")
file(GLOB_RECURSE TEST_SOURCES "test/*.c")

# Create main CPM library
add_library(cpm_static STATIC ${LIB_SOURCES})
target_link_libraries(cpm_static 
    ${PMDK_LIBRARIES}
    ${CJSON_LIBRARIES}
    Threads::Threads
    m
)

# Create shared library
add_library(cpm_shared SHARED ${LIB_SOURCES})
target_link_libraries(cpm_shared 
    ${PMDK_LIBRARIES}
    ${CJSON_LIBRARIES}
    Threads::Threads
    m
)
set_target_properties(cpm_shared PROPERTIES OUTPUT_NAME cpm)

# Create main CPM executable
add_executable(cpm cpm.c)
target_link_libraries(cpm cpm_static)

# Create CPX executable (package executor)
add_executable(cpx cpx.c)
target_link_libraries(cpx cpm_static)

# Create tests
enable_testing()

# Unit tests
add_executable(test_promises test/core/promise_test.c)
target_link_libraries(test_promises cpm_static)
add_test(NAME PromiseTests COMMAND test_promises)

add_executable(test_pmll test/core/pmll_test.c)
target_link_libraries(test_pmll cpm_static)
add_test(NAME PMllTests COMMAND test_pmll)

add_executable(test_package test/core/package_test.c)
target_link_libraries(test_package cpm_static)
add_test(NAME PackageTests COMMAND test_package)

# Command tests
add_executable(test_install test/commands/install_test.c)
target_link_libraries(test_install cpm_static)
add_test(NAME InstallTests COMMAND test_install)

# Install targets
install(TARGETS cpm cpx
    RUNTIME DESTINATION bin
)

install(TARGETS cpm_static cpm_shared
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

install(DIRECTORY include/
    DESTINATION include/cpm
    FILES_MATCHING PATTERN "*.h"
)

install(FILES cpm_package.spec
    DESTINATION share/cpm
)

# Create pkg-config file
configure_file(cpm.pc.in cpm.pc @ONLY)
install(FILES ${CMAKE_BINARY_DIR}/cpm.pc
    DESTINATION lib/pkgconfig
)

# Documentation with Doxygen
find_package(Doxygen)
if(DOXYGEN_FOUND)
    configure_file(docs/Doxyfile.in docs/Doxyfile @ONLY)
    add_custom_target(docs
        ${DOXYGEN_EXECUTABLE} ${CMAKE_BINARY_DIR}/docs/Doxyfile
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Generating API documentation with Doxygen"
        VERBATIM
    )
endif()

# CPack configuration
set(CPACK_PACKAGE_NAME "CPM")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "C Package Manager with Q Promises")
set(CPACK_PACKAGE_VENDOR "Dr. Q Josef Kurk Edwards")
set(CPACK_PACKAGE_CONTACT "spgaga24@gmail.com")

include(CPack)