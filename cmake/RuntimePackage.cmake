# Opt-in component: never install the entire build/bin directory or local data.
set(gpp_runtime_targets GuitarPP gpp_chart_studio gpp_score_to_chart
    gpp_dump_chart gpp_scan_library)
set(gpp_package_executables "")
foreach(target IN LISTS gpp_runtime_targets)
    if(NOT TARGET ${target})
        message(FATAL_ERROR "GPP_PACKAGE_RUNTIME requires BUILD_CHART_STUDIO and BUILD_CHART_TOOLS")
    endif()
    set_target_properties(${target} PROPERTIES INSTALL_RPATH "$ORIGIN/lib"
        INSTALL_RPATH_USE_LINK_PATH FALSE)
    install(TARGETS ${target} RUNTIME DESTINATION . COMPONENT Runtime)
    string(APPEND gpp_package_executables "\"$<TARGET_FILE:${target}>\"\n")
    if(MSVC)
        install(FILES "$<TARGET_PDB_FILE:${target}>" DESTINATION .
            CONFIGURATIONS RelWithDebInfo Debug COMPONENT Runtime)
    endif()
endforeach()

install(FILES src/Config.lua DESTINATION . COMPONENT Runtime)
install(FILES packaging/README.txt DESTINATION . COMPONENT Runtime)

# Only tracked authored assets. In particular, never include local songs/saves.
find_package(Git REQUIRED)
execute_process(COMMAND "${GIT_EXECUTABLE}" -c core.quotepath=false ls-files -- data
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    OUTPUT_VARIABLE gpp_assets OUTPUT_STRIP_TRAILING_WHITESPACE
    COMMAND_ERROR_IS_FATAL ANY)
string(REPLACE "\n" ";" gpp_assets "${gpp_assets}")
foreach(asset IN LISTS gpp_assets)
    if(NOT asset MATCHES "^data/(songs|saves|cache)/")
        get_filename_component(directory "${asset}" DIRECTORY)
        install(FILES "${PROJECT_SOURCE_DIR}/${asset}"
            DESTINATION "${directory}" COMPONENT Runtime)
    endif()
endforeach()

function(gpp_runtime_license source name)
    install(FILES "${source}" DESTINATION licenses RENAME "${name}.txt"
        COMPONENT Runtime)
endfunction()
gpp_runtime_license("${PROJECT_SOURCE_DIR}/README.md" GuitarPP)
gpp_runtime_license("${PROJECT_SOURCE_DIR}/glfw/LICENSE.md" glfw)
gpp_runtime_license("${PROJECT_SOURCE_DIR}/glm/copying.txt" glm)
gpp_runtime_license("${PROJECT_SOURCE_DIR}/cereal/LICENSE" cereal)
gpp_runtime_license("${PROJECT_SOURCE_DIR}/vendor/imgui/LICENSE.txt" imgui)
gpp_runtime_license("${PROJECT_SOURCE_DIR}/vendor/midifile/LICENSE.txt" midifile)
gpp_runtime_license("${PROJECT_SOURCE_DIR}/vendor/xxhash/LICENSE" xxhash)
gpp_runtime_license("${PROJECT_SOURCE_DIR}/src/sqlite3/LICENSE" sqlite3raii)
# lua.h includes the full Lua copyright and permission notice.
gpp_runtime_license("${PROJECT_SOURCE_DIR}/src/Lua/lua.h" Lua)
gpp_runtime_license("${PROJECT_SOURCE_DIR}/bass/bass.txt" bass)
gpp_runtime_license("${PROJECT_SOURCE_DIR}/bass/bass_fx.txt" bass_fx)

if(WIN32)
    install(FILES "$<TARGET_FILE:bass>" "$<TARGET_FILE:bass_fx>"
        DESTINATION . COMPONENT Runtime)
    foreach(port glew sqlite3 libpng zlib)
        gpp_runtime_license("${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/share/${port}/copyright" "${port}")
    endforeach()
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    configure_file("${CMAKE_CURRENT_LIST_DIR}/RuntimeLibraries.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/RuntimeLibraries.cmake.in" @ONLY)
    file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/RuntimeLibraries-$<CONFIG>.cmake"
        INPUT "${CMAKE_CURRENT_BINARY_DIR}/RuntimeLibraries.cmake.in")
    install(SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/RuntimeLibraries-$<CONFIG>.cmake"
        COMPONENT Runtime)
    foreach(package libglew-dev libsqlite3-dev libpng-dev zlib1g-dev)
        gpp_runtime_license("/usr/share/doc/${package}/copyright" "${package}")
    endforeach()
else()
    message(FATAL_ERROR "Runtime ZIPs currently support Linux and Windows only")
endif()
