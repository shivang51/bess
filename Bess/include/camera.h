#pragma once

#include "glm.hpp"
#include "types.h"

namespace Bess {
    struct CameraPositionAnimation {
        glm::vec2 startPos, endPos;
        TAnimationTime duration;
        TAnimationTime currentTime = TAnimationTime(0);
        bool finised = true;

        glm::vec2 getNextPos(TFrameTime ts) {
            if (currentTime >= duration) {
                finised = true;
            }

            float t = currentTime / duration;
            if (t > 1.f)
                t = 1.f;

            auto pos = glm::mix(startPos, endPos, t);
            currentTime += ts;
            return pos;
        }
    };

    struct CameraPositionZoomAnimation {
        glm::vec2 startPos, endPos;
        float startZoom, endZoom;
        TAnimationTime duration;
        TAnimationTime currentTime = TAnimationTime(0);
        bool finised = true;

        std::pair<glm::vec2, float> getNextVal(TFrameTime ts) {
            if (currentTime >= duration) {
                finised = true;
            }

            float t = currentTime / duration;
            if (t > 1.f)
                t = 1.f;

            auto pos = glm::mix(startPos, endPos, t);
            auto zoom = glm::mix(startZoom, endZoom, t);
            currentTime += ts;
            return {pos, zoom};
        }
    };

    class Camera {
      public:
        Camera(float width, float height);
        ~Camera();

        void update(TFrameTime ts);

        void setPos(const glm::vec2 &pos);
        glm::vec2 getPos() const;

        glm::vec2 &getPosRef();

        void setZoom(float zoom);

        void incrementZoom(float value);
        void incrementZoomToPoint(const glm::vec2 &point, float value);

        void focusAtPoint(const glm::vec2 &pos, bool smooth = true);

        float getZoom() const;
        float &getZoomRef();

        glm::vec2 getSpan() const;

        void resize(float width, float height);

        glm::mat4 getTransform() const;

        glm::mat4 getOrtho() const;

        void incrementPos(const glm::vec2 &pos);

        static float zoomMin, zoomMax, defaultZoom;

      private:
        glm::vec2 m_pos;
        float m_zoom;
        float m_width, m_height;
        glm::mat4 m_ortho;
        glm::mat4 transform;

        void updateTransform();
        void recalculateOrtho();

        CameraPositionAnimation m_posAnimation;
        CameraPositionZoomAnimation m_posZoomAnimation;
    };
} // namespace Bess
