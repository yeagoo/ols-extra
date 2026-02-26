cmake_minimum_required(VERSION 3.14)
project(ols-htaccess C CXX)

# C11 for module source, C++17 for tests
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Shared library output
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

# Collect all C source files in src/
file(GLOB MODULE_SOURCES src/*.c)

# Build the shared library (.so)
if(MODULE_SOURCES)
    add_library(ols_htaccess SHARED ${MODULE_SOURCES})
    target_include_directories(ols_htaccess PUBLIC ${CMAKE_SOURCE_DIR}/include)
    set_target_properties(ols_htaccess PROPERTIES
        OUTPUT_NAME "ols_htaccess"
        PREFIX ""
        SUFFIX ".so"
    )
endif()

# Testing dependencies via FetchContent
include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)

FetchContent_Declare(
    rapidcheck
    GIT_REPOSITORY https://github.com/emil-e/rapidcheck.git
    GIT_TAG master
)

# Prevent GoogleTest from overriding compiler/linker settings on Windows
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

# Enable RapidCheck Google Test integration (extras/gtest)
set(RC_ENABLE_GTEST ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googletest rapidcheck)

# Enable testing
enable_testing()

# Add test subdirectory
add_subdirectory(tests)
