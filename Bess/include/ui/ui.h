#pragma once

#include "fwd.hpp"
#include "glad/glad.h"
#include <GLFW/glfw3.h>
#include <map>
#include <memory>
#include <unordered_map>
#include <vector>

#include "application_state.h"
#include "camera.h"

namespace Bess::UI {

struct UIState {
    float cameraZoom = Camera::defaultZoom;
    glm::vec2 viewportSize = {800, 500};
    glm::vec2 viewportPos = {0, 0};
    GLuint64 viewportTexture = 0;
};

class UIMain {
  public:
    static void init(GLFWwindow *window);

    static void shutdown();

    static void draw();

    static void setViewportTexture(GLuint64 texture);

    static void setCursorPointer();

    static void setCursorReset();

    static UIState state;

  private:
      static void setModernColors();
      static void setMaterialYouColors();
      static void setDarkThemeColors();
      static void setBessDarkThemeColors();
    static void setCatpuccinMochaColors();

    static void drawProjectExplorer();
    static void drawViewport();
    static void drawPropertiesPanel();

    static void begin();
    static void end();


    static void resetDockspace();
};
} // namespace Bess
