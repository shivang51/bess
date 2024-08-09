#pragma once
#include "component.h"
#include "components/slot.h"
#include "glm.hpp"
#include <vector>

#include "uuid_v4.h"
#include "components_manager/jcomponent_data.h"


namespace Bess::Simulator::Components {
    class JComponent : public Component {
    public:
        JComponent();
        JComponent(const UUIDv4::UUID& uid, int renderId, glm::vec3 position,
            std::vector<UUIDv4::UUID> inputSlots,
            std::vector<UUIDv4::UUID> outputSlots, const std::shared_ptr<JComponentData> data);
        ~JComponent() = default;

        void render() override;

        void simulate();

        void generate(const glm::vec3& pos = { 0.f, 0.f, 0.f }) override;
        void generate(const std::shared_ptr<JComponentData> data, const glm::vec3& pos = { 0.f, 0.f, 0.f });

    private:
        std::vector<UUIDv4::UUID> m_inputSlots;
        std::vector<UUIDv4::UUID> m_outputSlots;
        const std::shared_ptr<JComponentData> m_data;


        void onLeftClick(const glm::vec2& pos);
        void onRightClick(const glm::vec2& pos);

    private:
        void drawBackground(const glm::vec4& borderRadiusPx, float rPx, float headerHeight);
    };
} // namespace Bess::Simulator::Components
