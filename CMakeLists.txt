set(myprojectname GuitarPP)

cmake_minimum_required(VERSION 3.6.0)
project(${myprojectname} VERSION 0.1.0)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 17)

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

include(CTest)
enable_testing()
#find_package(GTest REQUIRED)
#include(GoogleTest)

add_subdirectory(${CMAKE_SOURCE_DIR}/glfw)

# setando todos arquivos fontes que serão compilados
file(
    GLOB SOURCES 
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
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

add_executable(${myprojectname} ${SOURCES})

target_include_directories(${myprojectname} PRIVATE ${INCLUDE_DIRS})
target_link_directories(${myprojectname} PRIVATE ${INCLUDE_DIRS})

# Compiler-specific compile flags
if (${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU")
    message(STATUS "Setting G++ flags")
    target_compile_options(${myprojectname} PRIVATE -march=native -mtune=native -Wall -Wextra $<$<COMPILE_LANGUAGE:CXX>:-std=c++17> -Wformat-security -Wconversion -Wsign-conversion)
elseif(${CMAKE_CXX_COMPILER_ID} STREQUAL "MSVC")
    message(STATUS "Setting MSVC flags")
    target_compile_options(${myprojectname} PRIVATE /EHsc /W2 /c)
elseif(${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang")
    message(STATUS "Setting Clang flags")
    target_compile_options(${myprojectname} PRIVATE -march=native -mtune=native -Weverything -Wno-date-time -Wno-disabled-macro-expansion -Wno-global-constructors $<$<COMPILE_LANGUAGE:CXX>:-std=c++17>  -Wno-c++98-compat -Wno-c++98-compat-pedantic -Wno-padded -Wno-exit-time-destructors)
endif()

target_link_libraries(${myprojectname} glfw GLEW OpenGL::GL bass bass_fx)


