#pragma once
#include "device.h"
#include <memory>
#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"

#include "imgui.h"

namespace Bess::UI {
    static VkDescriptorPool s_uiDescriptorPool = VK_NULL_HANDLE;
    void init(GLFWwindow *window);
    void initVulkanImGui();
    void begin();
    void end();

    void drawStats(int fps);

    void vulkanCleanup(std::shared_ptr<Vulkan::VulkanDevice> device);
    void shutdown();
    void loadFontAndSetScale(float fontSize, float scale);
    void setCursorPointer();
    void setCursorMove();
    void setCursorNormal();

    enum class CursorType : uint8_t {
        pointer,
        move,
        normal
    } currentCursorType;

    class Fonts {
      public:
        static ImFont *largeFont;
        static ImFont *mediumFont;
    };

} // namespace Bess::UI
