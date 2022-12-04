

include(CTest)
enable_testing()
find_package(GTest REQUIRED)
include(GoogleTest)

file(GLOB TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

add_executable(tests ${TEST_SOURCES})
target_link_libraries(tests GuitarPP_lib GTest::GTest GTest::Main)

