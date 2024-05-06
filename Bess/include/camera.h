#pragma once

#include "glm.hpp"

namespace Bess {
class Camera {
  public:
    Camera();
    ~Camera();

    void setPos(const glm::vec2 &pos);
    glm::vec2 getPos() const;

    void setZoom(float zoom);

    void updateZoom(float value);
    void updateY(float value);
    void updateX(float value);

    float getZoom() const;

    void resize(float width, float height);

    glm::mat4 getTransform() const;

  private:
    glm::vec2 m_pos;
    float m_zoom;
    float m_aspectRatio;
    glm::mat4 m_ortho;
    glm::mat4 transform;

    void updateTransform();

    void recalculateOrtho();
};
} // namespace Bess
