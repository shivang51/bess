#include "camera.h"
#include "ext/matrix_clip_space.hpp"
#include "ext/matrix_transform.hpp"

namespace Bess {
    float Camera::zoomMin = 0.5f, Camera::zoomMax = 4.f, Camera::defaultZoom = 1.517f;

    Camera::Camera(float width, float height) : m_zoom(defaultZoom), m_width(width), m_height(height) {
        m_pos = {0.f, 0.f};
    }

    Camera::~Camera() {}

    void Camera::update(TFrameTime ts) {
        if (!m_posAnimation.finised) {
            setPos(m_posAnimation.getNextPos(ts));
        }
    }

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
        if (zoom > zoomMax)
            zoom = zoomMax;
        else if (zoom < zoomMin)
            zoom = zoomMin;
        m_zoom = zoom;
        recalculateOrtho();
    }

    void Camera::incrementZoom(float value) {
        setZoom(m_zoom + value);
    }

    void Camera::incrementZoomToPoint(const glm::vec2 &point, float value) {
        auto newZoom = m_zoom + value;
        auto oldZoom = m_zoom;
        setZoom(newZoom);
        if (m_zoom == oldZoom)
            return;
        m_pos += (point - m_pos) * (1.0f - oldZoom / newZoom);
        updateTransform();
    }

    void Camera::focusAtPoint(const glm::vec2 &pos, bool smooth) {
        setZoom(2.f);
        if (!smooth) {
            setPos(pos);
            return;
        }

        m_posAnimation = CameraPositionAnimation();
        m_posAnimation.startPos = m_pos;
        m_posAnimation.endPos = pos;
        m_posAnimation.duration = TAnimationTime(200);
        m_posAnimation.finised = false;
    }

    float Camera::getZoom() const { return m_zoom; }

    float &Camera::getZoomRef() { return m_zoom; }

    glm::vec2 Camera::getSpan() const {
        float xSpan = m_width / m_zoom;
        float ySpan = m_height / m_zoom;
        return {xSpan, ySpan};
    }

    void Camera::resize(float width, float height) {
        m_width = width;
        m_height = height;
        recalculateOrtho();
    }

    void Camera::recalculateOrtho() {
        float xSpan = m_width / m_zoom;
        float ySpan = m_height / m_zoom;
        auto x = xSpan / 2.f;
        auto y = ySpan / 2.f;
        m_ortho = glm::ortho(-x, x, y, -y, -10.0f, 10.0f);

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
