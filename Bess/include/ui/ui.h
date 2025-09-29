#pragma once
#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"

#include "imgui.h"

namespace Bess::UI {
    void init(GLFWwindow *window);
    void initVulkanImGui();
    void begin();
    void end();

    void drawStats(int fps);

    void shutdown();
    void loadFontAndSetScale(float fontSize, float scale);
    void setCursorPointer();
    void setCursorMove();
    void setCursorNormal();

    class Fonts {
      public:
        static ImFont *largeFont;
        static ImFont *mediumFont;
    };

} // namespace Bess::UI
