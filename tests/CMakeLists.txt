

include(CTest)
enable_testing()
find_package(GTest REQUIRED)
include(GoogleTest)

file(GLOB TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

add_executable(tests ${TEST_SOURCES})
target_link_libraries(tests GuitarPP_lib GTest::GTest GTest::Main)


add_test(NAME shader_ownership COMMAND tests --gtest_filter=ShaderOwnership.*:ParticleEmission.*)
find_program(XVFB_RUN xvfb-run)
if(UNIX AND NOT APPLE AND XVFB_RUN)
    add_test(NAME shader_gl COMMAND ${XVFB_RUN} -a $<TARGET_FILE:tests>
             --gtest_filter=ShaderGL.*)
    set_tests_properties(shader_gl PROPERTIES ENVIRONMENT
        "LIBGL_ALWAYS_SOFTWARE=1;MESA_GL_VERSION_OVERRIDE=2.0;MESA_GLSL_VERSION_OVERRIDE=110")
else()
    add_test(NAME shader_gl COMMAND tests --gtest_filter=ShaderGL.*)
endif()

if(UNIX AND NOT APPLE AND XVFB_RUN)
    add_test(NAME font_metrics COMMAND ${XVFB_RUN} -a $<TARGET_FILE:tests>
             --gtest_filter=FontMetricsGL.*:FontAtlasGL.*)
    set_tests_properties(font_metrics PROPERTIES ENVIRONMENT "LIBGL_ALWAYS_SOFTWARE=1")
else()
    add_test(NAME font_metrics COMMAND tests --gtest_filter=FontMetricsGL.*:FontAtlasGL.*)
endif()

add_test(NAME obj_loader COMMAND tests --gtest_filter=ObjLoader.*)
add_test(NAME settings COMMAND tests --gtest_filter=Settings.*)
add_test(NAME logging COMMAND tests --gtest_filter=Logging.*)
add_test(NAME filesystem COMMAND tests --gtest_filter=FileSystem.*)

if(UNIX AND NOT APPLE AND XVFB_RUN)
    add_test(NAME startup_errors COMMAND ${XVFB_RUN} -a ${CMAKE_COMMAND}
        -DGPP_EXECUTABLE=$<TARGET_FILE:GuitarPP>
        -DGPP_TEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/startup-errors
        -P ${CMAKE_CURRENT_SOURCE_DIR}/startup_errors.cmake)
endif()

add_test(NAME song_loading COMMAND tests --gtest_filter=SongLoading.*)

add_test(NAME scene_definitions COMMAND tests --gtest_filter=Scenes.*)
if(UNIX AND NOT APPLE AND XVFB_RUN)
    add_test(NAME scene_gl COMMAND ${XVFB_RUN} -a $<TARGET_FILE:tests>
             --gtest_filter=SceneGL.*)
    set_tests_properties(scene_gl PROPERTIES ENVIRONMENT
        "LIBGL_ALWAYS_SOFTWARE=1;MESA_GL_VERSION_OVERRIDE=2.0;MESA_GLSL_VERSION_OVERRIDE=110")
else()
    add_test(NAME scene_gl COMMAND tests --gtest_filter=SceneGL.*)
endif()

add_test(NAME ui_cpu COMMAND tests --gtest_filter=FontCPU.*:MenuCPU.*)
