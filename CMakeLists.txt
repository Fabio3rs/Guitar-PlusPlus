set(myprojectname GuitarPP)

cmake_minimum_required(VERSION 3.6.0)
project(${myprojectname} VERSION 0.1.0)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 17)
set(OpenGL_GL_PREFERENCE "LEGACY")

#set(CMAKE_CXX_CLANG_TIDY "clang-tidy;-header-filter=.;-extra-arg=-std=c++17;-export-fixes;-checks=read*,modern*,hicpp*,performance*")

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

include(CPack)

add_subdirectory(glm)

#if(POLICY CMP0072)
#    cmake_policy(SET CMP0072 NEW)
#endif()

#find_package(CURL REQUIRED)
find_package(PkgConfig)
find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)

add_subdirectory(${CMAKE_SOURCE_DIR}/glfw)

# setando todos arquivos fontes que serão compilados
file(
    GLOB SOURCES 
    "${CMAKE_CURRENT_SOURCE_DIR}/src/CCampaing.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/CChart.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/CCharter.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/CControls.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/CEngine.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/CFonts.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/CGPPFileMGR.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/CGamePlay.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/CGuitars.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/CLanguageManager.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/CLog.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/CLuaFunctions.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/CLuaH.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/CMenu.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/CMultiplayer.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/CParticle.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/CPlayer.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/CSaveSystem.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/CShader.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/CText.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/CVideo.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/GPPGame.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/GPPOBJ.cpp"
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

add_library("${myprojectname}_lib" STATIC ${SOURCES})
add_executable(${myprojectname} "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")

# Compiler-specific compile flags
if (${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU")
    message(STATUS "Setting G++ flags")
    target_compile_options("${myprojectname}_lib" PRIVATE -march=native -mtune=native -Wall -Wextra $<$<COMPILE_LANGUAGE:CXX>:-std=c++17> -Wformat-security -Wconversion -Wsign-conversion)
elseif(${CMAKE_CXX_COMPILER_ID} STREQUAL "MSVC")
    message(STATUS "Setting MSVC flags")
    target_compile_options("${myprojectname}_lib" PRIVATE /EHsc /W2 /c)
elseif(${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang")
    message(STATUS "Setting Clang flags")
    target_compile_options("${myprojectname}_lib" PRIVATE -march=native -mtune=native -Weverything -Wno-date-time -Wno-disabled-macro-expansion -Wno-global-constructors $<$<COMPILE_LANGUAGE:CXX>:-std=c++17>  -Wno-c++98-compat -Wno-c++98-compat-pedantic -Wno-padded -Wno-exit-time-destructors)
endif()

target_link_libraries("${myprojectname}_lib" glfw GLEW OpenGL::GL bass bass_fx)
target_link_libraries(${myprojectname} "${myprojectname}_lib")

add_subdirectory(tests)
