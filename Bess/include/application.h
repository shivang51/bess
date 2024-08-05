#pragma once

#include "camera.h"
#include "fwd.hpp"
#include "renderer/gl/framebuffer.h"
#include "window.h"
#include <memory>
#include "application_state.h"
#include "components/component.h"

namespace Bess
{

  class Application
  {
  public:
    Application();
    ~Application();

    void run();
    void quit();

    void update();

    bool isKeyPressed(int key);

  private:
    Window m_window;

    std::unique_ptr<Gl::FrameBuffer> m_framebuffer;
    std::shared_ptr<Camera> m_camera;

  private:
    void drawUI();
    void drawScene();

    bool isCursorInViewport() const;
    glm::vec2 getViewportMousePos() const;
    glm::vec2 getNVPMousePos();
    // callbacks
  private:
    void onWindowResize(int w, int h);
    void onMouseWheel(double x, double y);
    void onKeyPress(int key);
    void onKeyRelease(int key);
    void onLeftMouse(bool pressed);
    void onRightMouse(bool pressed);
    void onMiddleMouse(bool pressed);
    void onMouseMove(double x, double y);

  private:
    std::unordered_map<int, bool> m_pressedKeys;
    bool m_leftMousePressed = false;
    bool m_rightMousePressed = false;
    bool m_middleMousePressed = false;

    glm::vec2 m_mousePos = {0, 0};
  };
} // namespace Bess
