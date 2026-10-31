add_executable(gpp_sfxgen main.cpp)
target_link_libraries(gpp_sfxgen PRIVATE gpp_sfx)
# Explicit opt-in: players receive WAVs, never run the renderer at startup.
add_custom_target(gpp_sfx_library
    COMMAND gpp_sfxgen --all "${PROJECT_SOURCE_DIR}/data/sounds/specs"
        --check --require-filename-id
    COMMAND gpp_sfxgen --all "${PROJECT_SOURCE_DIR}/data/sounds/specs"
        --output-dir "${CMAKE_BINARY_DIR}/bin/data/sounds/generated" --force --require-filename-id
    DEPENDS gpp_sfxgen
    COMMENT "Rendering Guitar++ procedural sound library")
include(install.cmake)
gpp_install_sfx_library("${PROJECT_SOURCE_DIR}/data/sounds/specs"
    "${CMAKE_BINARY_DIR}/bin/data/sounds/generated")
if(BUILD_TESTING)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    add_test(NAME sfx_cli COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_cli.py
        $<TARGET_FILE:gpp_sfxgen> ${PROJECT_SOURCE_DIR}/data/sounds/specs)
    set_tests_properties(sfx_cli PROPERTIES LABELS "audio;sfx;cpu" TIMEOUT 120)
    add_test(NAME sfx_package COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_package.py
        $<TARGET_FILE:gpp_sfxgen> ${CMAKE_COMMAND} ${CMAKE_CURRENT_SOURCE_DIR}/install.cmake)
    set_tests_properties(sfx_package PROPERTIES LABELS "audio;sfx;cpu" TIMEOUT 60)
endif()
