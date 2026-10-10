// The pinned GLFW predates glfwGetPlatform but advertises 3.4. Restrict only
// this backend translation unit to the supported 3.3 feature set. No ABI or
// version reported by GLFW/the game is changed. Remove after a GLFW upgrade.
#include <GLFW/glfw3.h>
#if GLFW_VERSION_MAJOR == 3 && GLFW_VERSION_MINOR == 4 &&                      \
    !defined(GLFW_PLATFORM_X11)
#undef GLFW_VERSION_MINOR
#define GLFW_VERSION_MINOR 3
#undef GLFW_VERSION_REVISION
#define GLFW_VERSION_REVISION 0
#endif
#include "backends/imgui_impl_glfw.cpp"
