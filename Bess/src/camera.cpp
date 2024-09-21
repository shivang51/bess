#include "camera.h"
#include "ext/matrix_clip_space.hpp"
#include "ext/matrix_transform.hpp"

namespace Bess {
    float Camera::zoomMin = 0.5f, Camera::zoomMax = 4.f, Camera::defaultZoom = 1.517f;

    Camera::Camera(float width, float height) : m_zoom(1.0f), m_width(width), m_height(height) {
        m_aspectRatio = width / height;
        m_pos = {0.f, 0.f};
    }

    Camera::~Camera() {}

    void Camera::setPos(const glm::vec2 &pos) {
        m_pos = pos;
        updateTransform();
    }

    void Camera::incrementPos(const glm::vec2 &pos) {
        m_pos += pos;
        updateTransform();
    }

    glm::vec2 Camera::getPos() const { return m_pos; }

    glm::vec2 &Camera::getPosRef() { return m_pos; }

    void Camera::setZoom(float zoom) {
        m_zoom = zoom;
        recalculateOrtho();
    }

    void Camera::updateZoom(float value) {
        m_zoom += value;
        recalculateOrtho();
    }

    void Camera::zoomToPoint(const glm::vec2 &point, float value) {
        m_pos += (point - m_pos) * (1.0f - value / m_zoom);
        m_zoom = value;
        recalculateOrtho();
    }

    float Camera::getZoom() const { return m_zoom; }

    glm::vec2 Camera::getSpan() const {
        float xSpan = m_width / m_zoom;
        float ySpan = m_height / m_zoom;
        return {xSpan, ySpan};
    }

    void Camera::resize(float width, float height) {
        m_width = width;
        m_height = height;
        m_aspectRatio = width / height;
        m_zoom = m_zoom / m_aspectRatio;
        recalculateOrtho();
    }

    void Camera::recalculateOrtho() {
        float xSpan = m_width / m_zoom;
        float ySpan = m_height / m_zoom;
        auto x = xSpan / 2.f;
        auto y = ySpan / 2.f;
        m_ortho = glm::ortho(-x, x, -y, y, -10.0f, 10.0f);
        updateTransform();
    }

    glm::mat4 Camera::getTransform() const { return transform; }

    glm::mat4 Camera::getOrtho() const {
        return m_ortho;
    }

    void Camera::updateTransform() {
        transform = glm::translate(glm::mat4(1.f), glm::vec3(-m_pos, 0.0f));
        transform = m_ortho * transform;
    }

} // namespace Bess
