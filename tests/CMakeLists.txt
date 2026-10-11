find_package(GTest REQUIRED)
include(GoogleTest)

file(GLOB TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
list(FILTER TEST_SOURCES EXCLUDE REGEX "/texture_manager_tests\\.cpp$")

# This executable never links the game, GL, GLFW, BASS or Lua.
add_executable(gpp_texture_tests texture_manager_tests.cpp)
target_link_libraries(gpp_texture_tests PRIVATE gpp_textures GTest::GTest GTest::Main)
gtest_discover_tests(gpp_texture_tests
    TEST_PREFIX "texture_core."
    PROPERTIES LABELS cpu TIMEOUT 30
    DISCOVERY_TIMEOUT 30)

add_executable(tests ${TEST_SOURCES})
target_link_libraries(tests GuitarPP_lib GTest::GTest GTest::Main)

find_program(XVFB_RUN xvfb-run)
if(UNIX AND NOT APPLE AND XVFB_RUN)
    # Only GL cases need Xvfb; discovery and CPU tests run without a display.
    set(test_launcher "${CMAKE_COMMAND};-DXVFB_RUN=${XVFB_RUN};-P;${CMAKE_CURRENT_SOURCE_DIR}/run_gtest.cmake;--")
    if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.29")
        set_property(TARGET tests PROPERTY TEST_LAUNCHER "${test_launcher}")
    else()
        # CMake 3.16 (CI) also applies this launcher to native test targets.
        set_property(TARGET tests PROPERTY CROSSCOMPILING_EMULATOR "${test_launcher}")
    endif()
endif()

gtest_discover_tests(tests
    TEST_LIST discovered_tests
    DISCOVERY_TIMEOUT 30
)

# Runs after the generated discovery file, when individual names are available.
configure_file(discovered_properties.cmake.in discovered_properties.cmake @ONLY)
set_property(DIRECTORY APPEND PROPERTY TEST_INCLUDE_FILES
    "${CMAKE_CURRENT_BINARY_DIR}/discovered_properties.cmake")

if(UNIX AND NOT APPLE AND XVFB_RUN)
    add_test(NAME startup_errors COMMAND ${XVFB_RUN} -a ${CMAKE_COMMAND}
        -DGPP_EXECUTABLE=$<TARGET_FILE:GuitarPP>
        -DGPP_TEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/startup-errors
        -P ${CMAKE_CURRENT_SOURCE_DIR}/startup_errors.cmake)
    set_tests_properties(startup_errors PROPERTIES LABELS "integration;gpu"
        RESOURCE_LOCK gpp_graphics)
endif()
