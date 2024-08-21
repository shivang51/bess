#pragma once

/// dragable connection point

#include "components/component.h"
#include "glm.hpp"
#include "json.hpp"

namespace Bess::Simulator::Components {
    class ConnectionPoint : public Component {
    public:
        ConnectionPoint(const uuids::uuid& uid, const uuids::uuid& parentId, int renderId, const glm::vec3& pos);
        ~ConnectionPoint() = default;
        ConnectionPoint() = default;

        void render() override;

        void deleteComponent() override;

        void generate(const glm::vec3& pos = { 0.f, 0.f, 0.f }) override;

        void drawProperties() override;

        static void fromJson(const nlohmann::json& j);
        nlohmann::json toJson();

    private:
        void onLeftClick(const glm::vec2& pos);

    private:
        uuids::uuid m_parentId;
    };
}// namespace Bess::Simulator::Components