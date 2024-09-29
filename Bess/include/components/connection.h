#pragma once
#include "component.h"
#include "ext/vector_float3.hpp"
#include "settings/viewport_theme.h"
#include <vector>

namespace Bess::Simulator::Components {
    enum class ConnectionType {
        straight,
        curve
    };

    class Connection : public Component {
      public:
        Connection(const uuids::uuid &uid, int renderId, const uuids::uuid &slot1,
                   const uuids::uuid &slot2);
        ~Connection() = default;
        Connection();

        void render() override;
        void update() override;

        void deleteComponent() override;

        static uuids::uuid generate(const uuids::uuid &slot1, const uuids::uuid &slot2, const glm::vec3 &pos = {0.f, 0.f, 0.f});
        void generate(const glm::vec3 &pos = {0.f, 0.f, 0.f}) override;

        void drawProperties() override;

        const std::vector<uuids::uuid> &getPoints();
        void setPoints(const std::vector<glm::vec3> &points);
        void addPoint(const uuids::uuid &point);
        void removePoint(const uuids::uuid &point);
        void renderCurveConnection(glm::vec3 startPos, glm::vec3 endPos, float weight, glm::vec4 color);
        void renderStraightConnection(glm::vec3 startPos, glm::vec3 endPos, float weight, glm::vec4 color);

      private:
        uuids::uuid m_slot1;
        uuids::uuid m_slot2;

        void onLeftClick(const glm::vec2 &pos);
        void onFocusLost();
        void onFocus();
        void onMouseHover();

        void createConnectionPoint(const glm::vec2 &pos);

        std::vector<uuids::uuid> m_points = {};

      private: // properties
        glm::vec4 m_color = ViewportTheme::wireColor;
        ConnectionType m_type = ConnectionType::straight;
    };
} // namespace Bess::Simulator::Components
