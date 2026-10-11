set(imgui_root ${PROJECT_SOURCE_DIR}/vendor/imgui)
if(NOT EXISTS ${imgui_root}/imgui.cpp)
    message(FATAL_ERROR "The Studios require the existing vendor/imgui submodule. Initialize it explicitly.")
endif()
add_library(gpp_studio_imgui STATIC
    ${imgui_root}/imgui.cpp ${imgui_root}/imgui_draw.cpp
    ${imgui_root}/imgui_tables.cpp ${imgui_root}/imgui_widgets.cpp
    ${imgui_root}/backends/imgui_impl_opengl3.cpp glfw_backend.cpp)
target_include_directories(gpp_studio_imgui PUBLIC ${imgui_root} ${imgui_root}/backends)
target_link_libraries(gpp_studio_imgui PUBLIC glfw PRIVATE ${CMAKE_DL_LIBS})
