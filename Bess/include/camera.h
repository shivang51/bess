#pragma once

#include "glm.hpp"

namespace Bess
{
  class Camera
  {
  public:
    Camera(float width, float height);
    ~Camera();

    void setPos(const glm::vec2 &pos);
    glm::vec2 getPos() const;

    glm::vec2 &getPosRef();

    void setZoom(float zoom);

    void updateZoom(float value);

    float getZoom() const;

    void resize(float width, float height);

    glm::mat4 getTransform() const;

    void incrementPos(const glm::vec2 &pos);

    static float zoomMin, zoomMax, defaultZoom;

  private:
    glm::vec2 m_pos;
    float m_zoom;
    float m_aspectRatio;
    float m_width, m_height;
    glm::mat4 m_ortho;
    glm::mat4 transform;

    void updateTransform();

    void recalculateOrtho();
  };
} // namespace Bess
