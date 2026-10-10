

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
             --gtest_filter=FontMetricsGL.*)
    set_tests_properties(font_metrics PROPERTIES ENVIRONMENT "LIBGL_ALWAYS_SOFTWARE=1")
else()
    add_test(NAME font_metrics COMMAND tests --gtest_filter=FontMetricsGL.*)
endif()
