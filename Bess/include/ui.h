#pragma once

#include "components/component.h"
#include "fwd.hpp"
#include "glad/glad.h"
#include "glm.hpp"
#include "imgui.h"
#include <GLFW/glfw3.h>
#include <map>
#include <memory>
#include <unordered_map>
#include <vector>

#include "application_state.h"

namespace Bess {

struct UIState {
    float cameraZoom = 1.f;
    glm::vec2 viewportSize = {800, 500};
    glm::vec2 viewportPos = {0, 0};
    GLuint64 viewportTexture = 0;
};

class UI {
  public:
    static void init(GLFWwindow *window);

    static void shutdown();

    static void draw();

    static void setViewportTexture(GLuint64 texture);

    static void setCursorPointer();
    static void setCursorReset();

    static UIState state;

  private:
    static void setDarkThemeColors();

    static void drawProjectExplorer();
    static void drawViewport();
    static void drawComponentExplorer();
    static void drawPropertiesPanel();

    static void begin();
    static void end();

    static void resetDockspace();

    static std::map<std::string, std::function<void(const glm::vec3 &)>>
        m_components;
};
} // namespace Bess
