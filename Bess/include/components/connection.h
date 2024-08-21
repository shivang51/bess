#pragma once
#include "component.h"
#include "components_manager/components_manager.h"
#include "renderer/renderer.h"
#include "common/theme.h"
#include <vector>

namespace Bess::Simulator::Components {
    class Connection : public Component {
    public:
        Connection(const uuids::uuid& uid, int renderId, const uuids::uuid& slot1,
            const uuids::uuid& slot2);
        ~Connection() = default;
        Connection();

        void render() override;

        void deleteComponent() override;

        static void generate(const uuids::uuid& slot1, const uuids::uuid& slot2, const glm::vec3& pos = { 0.f, 0.f, 0.f });
        void generate(const glm::vec3& pos = { 0.f, 0.f, 0.f }) override;

        void drawProperties() override;

        const std::vector<uuids::uuid>& getPoints();
        void setPoints(const std::vector<uuids::uuid>& points);
        void addPoint(const uuids::uuid& point);
        void removePoint(const uuids::uuid& point);

    private:
        uuids::uuid m_slot1;
        uuids::uuid m_slot2;

        void onLeftClick(const glm::vec2& pos);
        void onFocusLost();
        void onFocus();
        void onMouseHover();

        std::vector<uuids::uuid> m_points = {};

    private: // properties
        glm::vec3 m_color = Theme::wireColor;
    };
}// namespace Bess::Simulator::Components
