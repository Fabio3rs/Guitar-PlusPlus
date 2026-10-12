find_package(PNG REQUIRED)
add_library(gpp_gui_studio_core STATIC fonts.cpp session.cpp)
target_link_libraries(gpp_gui_studio_core PUBLIC GuitarPP_lib)
target_include_directories(gpp_gui_studio_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
add_executable(gpp_gui_studio main.cpp ui.cpp
    ${PROJECT_SOURCE_DIR}/vendor/imgui/misc/cpp/imgui_stdlib.cpp)
target_include_directories(gpp_gui_studio SYSTEM PRIVATE ${PROJECT_SOURCE_DIR}/vendor/imgui/misc/cpp)
target_link_libraries(gpp_gui_studio PRIVATE gpp_gui_studio_core gpp_studio_imgui PNG::PNG)
target_compile_definitions(gpp_gui_studio PRIVATE GPP_GUI_WORKSPACE="${PROJECT_SOURCE_DIR}")
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(gpp_gui_studio_core PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(gpp_gui_studio PRIVATE -Wall -Wextra -Wpedantic)
endif()
if(BUILD_TESTING)
    find_package(GTest REQUIRED)
    find_program(GUI_XVFB xvfb-run)
    add_executable(gpp_gui_tests tests.cpp)
    target_link_libraries(gpp_gui_tests PRIVATE gpp_gui_studio_core GTest::GTest GTest::Main)
    if(GUI_XVFB)
        add_test(NAME gui_studio COMMAND ${GUI_XVFB} -a $<TARGET_FILE:gpp_gui_tests>)
        set_tests_properties(gui_studio PROPERTIES TIMEOUT 60 LABELS "gui;gpu" RESOURCE_LOCK gpp_graphics)
    endif()
endif()
if(BUILD_TESTING AND GUI_XVFB)
    find_program(GUI_XDOTOOL xdotool)
    if(GUI_XDOTOOL)
        find_package(Python3 COMPONENTS Interpreter REQUIRED)
        add_test(NAME gui_studio_ui COMMAND ${GUI_XVFB} -a
            -s "-screen 0 1280x1024x24" ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/test_ui.py $<TARGET_FILE:gpp_gui_studio>
            ${CMAKE_BINARY_DIR}/gui-studio/tests)
        # Real mouse/keyboard input needs rendered frames between actions.
        # Avoid CPU test contention and excessive Mesa workers on small runners.
        set_tests_properties(gui_studio_ui PROPERTIES TIMEOUT 90 LABELS "gui;gpu"
            RESOURCE_LOCK gpp_graphics RUN_SERIAL TRUE
            ENVIRONMENT "LIBGL_ALWAYS_SOFTWARE=1;LP_NUM_THREADS=2")
    endif()
endif()
