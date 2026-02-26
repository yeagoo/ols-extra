# Test configuration for ols-htaccess module
# Unit tests use Google Test, property tests use RapidCheck + Google Test

include(GoogleTest)

# Include directories for test helpers and generators
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/tests
    ${CMAKE_SOURCE_DIR}/tests/generators
)

# Collect module source files for linking into tests
file(GLOB MODULE_SOURCES ${CMAKE_SOURCE_DIR}/src/*.c)

# Mock LSIAPI layer (shared by all test targets)
set(MOCK_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/mock_lsiapi.cpp)

# --- Unit Tests ---
file(GLOB UNIT_TEST_SOURCES unit/*.cpp)
if(UNIT_TEST_SOURCES)
    add_executable(unit_tests ${UNIT_TEST_SOURCES} ${MODULE_SOURCES} ${MOCK_SOURCES})
    target_link_libraries(unit_tests
        GTest::gtest
        GTest::gtest_main
        crypt
    )
    gtest_discover_tests(unit_tests)
endif()

# --- Property Tests ---
file(GLOB PROPERTY_TEST_SOURCES property/*.cpp)
if(PROPERTY_TEST_SOURCES)
    add_executable(property_tests ${PROPERTY_TEST_SOURCES} ${MODULE_SOURCES} ${MOCK_SOURCES})
    target_link_libraries(property_tests
        GTest::gtest
        GTest::gtest_main
        rapidcheck
        rapidcheck_gtest
        crypt
    )
    gtest_discover_tests(property_tests)
endif()

# --- Compatibility Tests ---
file(GLOB COMPAT_TEST_SOURCES compat/*.cpp)
if(COMPAT_TEST_SOURCES)
    add_executable(compat_tests ${COMPAT_TEST_SOURCES} ${MODULE_SOURCES} ${MOCK_SOURCES})
    target_compile_definitions(compat_tests PRIVATE
        COMPAT_SAMPLES_DIR="${CMAKE_SOURCE_DIR}/tests/compat/htaccess_samples"
    )
    target_link_libraries(compat_tests
        GTest::gtest
        GTest::gtest_main
        crypt
    )
    gtest_discover_tests(compat_tests)
endif()
