set(myprojectname GuitarPP)

cmake_minimum_required(VERSION 3.25)
project(${myprojectname} VERSION 0.1.0)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 17)
set(OpenGL_GL_PREFERENCE "LEGACY")

#set(CMAKE_CXX_CLANG_TIDY "clang-tidy;-header-filter=.;-extra-arg=-std=c++17;-export-fixes;-checks=read*,modern*,hicpp*,performance*")

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Small authored UI assets; game/music assets still come from the runtime pack.
foreach(icon folders artist music charter no-cover)
    configure_file(data/sprites/quick-play/${icon}.tga
        bin/data/sprites/quick-play/${icon}.tga COPYONLY)
endforeach()

include(CPack)

add_subdirectory(glm SYSTEM)

# Keep upstream settings scoped here. EXCLUDE_FROM_ALL also covers midi2beep
# and optional humdrum examples, which bypass BUILD_MIDILIBRARY_ONLY upstream.
set(BUILD_MIDILIBRARY_ONLY ON CACHE BOOL "Only build the MIDI library" FORCE)
add_subdirectory(vendor/midifile EXCLUDE_FROM_ALL SYSTEM)
target_include_directories(midifile SYSTEM PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/vendor/midifile/include")
set_target_properties(midifile PROPERTIES EXCLUDE_FROM_ALL FALSE)
configure_file(vendor/midifile/LICENSE.txt bin/licenses/midifile-LICENSE.txt COPYONLY)
install(FILES vendor/midifile/LICENSE.txt DESTINATION share/GuitarPP/licenses RENAME midifile-LICENSE.txt)

#if(POLICY CMP0072)
#    cmake_policy(SET CMP0072 NEW)
#endif()

#find_package(CURL REQUIRED)
find_package(PkgConfig)
find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)
# Linux: install libsqlite3-dev. Windows: provide SQLite3_INCLUDE_DIR and
# SQLite3_LIBRARY (matching the build architecture), or use a package-manager
# toolchain such as vcpkg. SQLite is a system dependency, not downloaded here.
find_package(SQLite3 REQUIRED)

add_subdirectory(${CMAKE_SOURCE_DIR}/glfw SYSTEM)

# setando todos arquivos fontes que serão compilados
file(
    GLOB SOURCES 
    "${CMAKE_CURRENT_SOURCE_DIR}/src/CCampaing.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/CChart.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/SongLibrary.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/SongLibraryAudio.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/SongFiles.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/CCharter.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/CControls.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/CEngine.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/CEngineResources.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/CUIRendererGL2.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/CFonts.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/CGPPFileMGR.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/CGamePlay.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/CGameSettings.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/CGuitars.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/CLanguageManager.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/CLog.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/CLuaFunctions.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/CLuaH.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/CMenu.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/MenuBackground.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/CMenuSystem.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/MenuPresentation.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/InitialMenus.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/SelectionMenus.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/QuickPlayView.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/CampaignMenus.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/DeveloperMenus.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/PauseMenu.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/CUIDefinition.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/CUIOverrides.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/CModalManager.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/CUIStyle.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/CMultiplayer.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/CParticle.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/CPlusLightning.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/CPlayer.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/CShader.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/CText.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/CVideo.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/GPPGame.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/GPPTexture.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/GPPModel.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/GPPOBJ.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/CBackgroundRenderer.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/CScene.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/exception.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/tga.cpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/src/Lua/*.c"
)

# setando todos arquivos include
set(INCLUDE_DIRS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/"
    "${CMAKE_CURRENT_SOURCE_DIR}/glm/"
    "${CMAKE_CURRENT_SOURCE_DIR}/cereal/include/"
    "${CMAKE_CURRENT_SOURCE_DIR}/bass/"
)

message(${INCLUDE_DIRS})

include_directories(SYSTEM ${INCLUDE_DIRS})
link_directories(${INCLUDE_DIRS})

find_package(Threads REQUIRED)
add_library(gpp_midi_input STATIC src/MidiInput.cpp)
target_include_directories(gpp_midi_input PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
add_library(gpp_sqlite STATIC src/sqlite3/sqlite3raii.cpp)
target_link_libraries(gpp_sqlite PUBLIC SQLite::SQLite3)
target_include_directories(gpp_sqlite PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
add_library(gpp_charter STATIC src/charter/Document.cpp)
target_include_directories(gpp_charter PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
add_library(gpp_midi STATIC src/CMidiChart.cpp)
target_compile_features(gpp_midi PUBLIC cxx_std_17)
target_include_directories(gpp_midi PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src" "${CMAKE_CURRENT_SOURCE_DIR}/cereal/include")
target_link_libraries(gpp_midi PRIVATE midifile gpp_midi_input)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(gpp_midi PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_library(gpp_models STATIC src/CModelData.cpp src/CModelManager.cpp src/ObjModel.cpp src/SceneAO.cpp)
target_include_directories(gpp_models PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(gpp_models PUBLIC Threads::Threads)

add_library(gpp_textures STATIC src/CTextureManager.cpp)
target_include_directories(gpp_textures PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(gpp_textures PUBLIC Threads::Threads)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(gpp_textures PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_library("${myprojectname}_lib" STATIC ${SOURCES})
target_include_directories("${myprojectname}_lib" SYSTEM PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/vendor/xxhash")
add_executable(${myprojectname} "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")

option(GPP_NATIVE_CPU "Optimize for the build machine (not for distribution)" OFF)
if(GPP_NATIVE_CPU AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options("${myprojectname}_lib" PRIVATE -march=native -mtune=native)
endif()

# Compiler-specific compile flags
if (${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU")
    message(STATUS "Setting G++ flags")
    target_compile_options("${myprojectname}_lib" PRIVATE -Wall -Wextra $<$<COMPILE_LANGUAGE:CXX>:-std=c++17> -Wformat-security -Wconversion -Wsign-conversion)
elseif(${CMAKE_CXX_COMPILER_ID} STREQUAL "MSVC")
    message(STATUS "Setting MSVC flags")
    target_compile_options("${myprojectname}_lib" PRIVATE /EHsc /W2 /c)
elseif(${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang")
    message(STATUS "Setting Clang flags")
    target_compile_options("${myprojectname}_lib" PRIVATE -Weverything -Wno-date-time -Wno-disabled-macro-expansion -Wno-global-constructors $<$<COMPILE_LANGUAGE:CXX>:-std=c++17>  -Wno-c++98-compat -Wno-c++98-compat-pedantic -Wno-padded -Wno-exit-time-destructors)
endif()

target_link_libraries("${myprojectname}_lib" gpp_midi gpp_textures gpp_models gpp_sqlite gpp_charter glfw GLEW OpenGL::GL bass bass_fx)
target_link_libraries(${myprojectname} "${myprojectname}_lib")


include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

option(BUILD_SCENE_TOOLS "Build offline scene preparation tools" OFF)
if(BUILD_SCENE_TOOLS)
    add_executable(gpp_bake_ao tools/bake_scene_ao.cpp)
    target_link_libraries(gpp_bake_ao GuitarPP_lib)
    add_executable(gpp_preview_background tools/preview_background.cpp)
    target_link_libraries(gpp_preview_background GuitarPP_lib)
endif()

option(BUILD_UI_TOOLS "Build optional UI benchmark" OFF)
option(BUILD_GAMEPLAY_BENCHMARK "Build controlled gameplay benchmark" OFF)
option(GPP_PROFILE_GAMEPLAY "Collect bounded per-thread gameplay timing samples" OFF)
if(GPP_PROFILE_GAMEPLAY)
    target_compile_definitions(GuitarPP_lib PUBLIC GPP_PROFILE_GAMEPLAY=1)
endif()
if(BUILD_GAMEPLAY_BENCHMARK)
    add_executable(gpp_benchmark_gameplay tools/benchmark_gameplay.cpp)
    target_link_libraries(gpp_benchmark_gameplay GuitarPP_lib)
endif()
if(BUILD_UI_TOOLS)
    add_executable(gpp_benchmark_ui tools/benchmark_ui.cpp)
    target_link_libraries(gpp_benchmark_ui GuitarPP_lib)
    target_compile_definitions(gpp_benchmark_ui PRIVATE GPP_UI_CURRENT=1)
endif()

option(BUILD_CHART_TOOLS "Build the CPU-only runtime chart dump tool" OFF)
if(BUILD_CHART_TOOLS)
    add_executable(gpp_scan_library tools/scan_library.cpp)
    target_link_libraries(gpp_scan_library GuitarPP_lib)
    add_executable(gpp_dump_chart tools/dump_chart.cpp)
    target_link_libraries(gpp_dump_chart GuitarPP_lib)
    if(BUILD_TESTING)
        find_package(Python3 COMPONENTS Interpreter REQUIRED)
        add_test(NAME chart_plus_statistics COMMAND ${Python3_EXECUTABLE}
            ${PROJECT_SOURCE_DIR}/tools/chart_studio/test_plus_stats.py
            --dumper $<TARGET_FILE:gpp_dump_chart>)
        set_tests_properties(chart_plus_statistics PROPERTIES
            LABELS "chart_studio;cpu" TIMEOUT 120)
    endif()
endif()

option(BUILD_SCENE_STUDIO "Build the independent Scene Studio" OFF)
option(BUILD_AUDIO_STUDIO "Build the independent Audio Studio" OFF)
option(BUILD_GUI_STUDIO "Build the independent GUI Studio" OFF)
option(BUILD_CHART_STUDIO "Build the independent Chart Studio and score CLI" OFF)
if(BUILD_SCENE_STUDIO OR BUILD_AUDIO_STUDIO OR BUILD_GUI_STUDIO OR BUILD_CHART_STUDIO)
    add_subdirectory(tools/scene_studio/imgui SYSTEM)
endif()
if(BUILD_SCENE_STUDIO)
    add_subdirectory(tools/scene_studio)
endif()
if(BUILD_AUDIO_STUDIO OR BUILD_CHART_STUDIO)
    add_subdirectory(src/audio)
endif()
if(BUILD_AUDIO_STUDIO)
    add_subdirectory(tools/audio_studio)
endif()

if(BUILD_GUI_STUDIO)
    add_subdirectory(tools/gui_studio)
endif()

install(FILES src/sqlite3/LICENSE DESTINATION share/GuitarPP/licenses RENAME sqlite3raii-LICENSE.txt)
install(FILES vendor/xxhash/LICENSE DESTINATION share/GuitarPP/licenses RENAME xxhash-LICENSE.txt)
configure_file(src/sqlite3/LICENSE bin/licenses/sqlite3raii-LICENSE.txt COPYONLY)
configure_file(vendor/xxhash/LICENSE bin/licenses/xxhash-LICENSE.txt COPYONLY)

if(BUILD_CHART_STUDIO)
    add_subdirectory(tools/chart_studio)
endif()
