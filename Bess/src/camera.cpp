#include "camera.h"
#include "common/log.h"
#include "ext/matrix_clip_space.hpp"
#include "ext/matrix_transform.hpp"

namespace Bess {
    float Camera::zoomMin = 0.5f, Camera::zoomMax = 4.f, Camera::defaultZoom = 1.517f;

    Camera::Camera(float width, float height) : m_zoom(defaultZoom), m_zPos(8.f), m_width(width), m_height(height) {
        setZoom(defaultZoom);
        setPos({0.f, 0.f});
    }

    Camera::~Camera() {}

    void Camera::update(TFrameTime ts) {
        if (!m_posAnimation.finised) {
            setPos(m_posAnimation.getNextPos(ts));
        }

        if (!m_posZoomAnimation.finised) {
            const auto &[pos, zoom] = m_posZoomAnimation.getNextVal(ts);
            setPos(pos);
            setZoom(zoom);
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

    const glm::vec2 &Camera::getPos() const { return m_pos; }

    glm::vec2 &Camera::getPosRef() { return m_pos; }

    glm::vec2 Camera::getSize() const {
        return {m_width, m_height};
    }

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
        const auto newZoom = m_zoom + value;
        const auto oldZoom = m_zoom;
        setZoom(newZoom);
        if (m_zoom == oldZoom)
            return;
        m_pos += (point - m_pos) * (1.0f - oldZoom / newZoom);
        updateTransform();
    }

    void Camera::focusAtPoint(const glm::vec2 &pos, bool smooth) {
        if (!smooth) {
            setZoom(2.f);
            setPos(pos);
            return;
        }

        m_posZoomAnimation = {};
        m_posZoomAnimation.startPos = m_pos;
        m_posZoomAnimation.endPos = pos;
        m_posZoomAnimation.startZoom = m_zoom;
        m_posZoomAnimation.endZoom = 2.f;
        m_posZoomAnimation.duration = TAnimationTime(100);
        m_posZoomAnimation.finised = false;
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
        const float xSpan = m_width / m_zoom;
        const float ySpan = m_height / m_zoom;
        const auto x = xSpan / 2.f;
        const auto y = ySpan / 2.f;
        m_ortho = glm::ortho(-x, x, y, -y, -m_zPos, m_zPos);

        updateTransform();
    }

    const glm::mat4 &Camera::getTransform() const { return transform; }

    const glm::mat4 &Camera::getOrtho() const {
        return m_ortho;
    }

    void Camera::updateTransform() {
        transform = glm::translate(glm::mat4(1.f), glm::vec3(-m_pos, -m_zPos));
        transform = m_ortho * transform;
    }

    void Camera::setZPos(float zPos) {
        m_zPos = zPos;
        updateTransform();
    }

    float Camera::getZPos() const {
        return m_zPos;
    }
} // namespace Bess
