#include "camera.h"
#include "ext/matrix_clip_space.hpp"
#include "ext/matrix_transform.hpp"

namespace Bess {
Camera::Camera() : m_pos(0.0f), m_zoom(1.0f) {}

Camera::~Camera() {}

void Camera::setPos(const glm::vec2 &pos) {
    m_pos = {-pos.x, pos.y};
    updateTransform();
}

glm::vec2 Camera::getPos() const { return m_pos; }

void Camera::setZoom(float zoom) {
    m_zoom = zoom;
    recalculateOrtho();
    updateTransform();
}

void Camera::updateZoom(float value) {
    m_zoom += value;
    if (m_zoom < 0.1f) {
        m_zoom = 0.1f;
        return;
    }
    recalculateOrtho();
    updateTransform();
}

void Camera::updateX(float value) {
    m_pos.x += value;
    updateTransform();
}

void Camera::updateY(float value) {
    m_pos.y += value;
    updateTransform();
}

float Camera::getZoom() const { return m_zoom; }

void Camera::resize(float width, float height) {
    m_aspectRatio = width / height;
    m_zoom = m_zoom / m_aspectRatio;
    recalculateOrtho();
}

void Camera::recalculateOrtho() {
    float xSpan = m_aspectRatio / m_zoom;
    float ySpan = 1.0f / m_zoom;
    m_ortho = glm::ortho(-xSpan, xSpan, -ySpan, ySpan, -1.0f, 1.0f);
    updateTransform();
}

glm::mat4 Camera::getTransform() const { return transform; }

void Camera::updateTransform() {
    transform = glm::translate(glm::mat4(1.f), glm::vec3(m_pos, 0.0f));
    transform = m_ortho * transform;
}

} // namespace Bess
