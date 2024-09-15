#pragma once
#include "component.h"
#include "components/slot.h"
#include "glm.hpp"
#include <vector>

#include "components_manager/jcomponent_data.h"
#include "uuid.h"

namespace Bess::Simulator::Components {
    class JComponent : public Component {
      public:
        JComponent();
        JComponent(const uuids::uuid &uid, int renderId, glm::vec3 position,
                   std::vector<uuids::uuid> inputSlots,
                   std::vector<uuids::uuid> outputSlots, const std::shared_ptr<JComponentData> data);
        ~JComponent() = default;

        void render() override;

        void simulate() override;

        void generate(const glm::vec3 &pos = {0.f, 0.f, 0.f}) override;

        void generate(const std::shared_ptr<JComponentData> data, const glm::vec3 &pos = {0.f, 0.f, 0.f});

        void deleteComponent() override;

        nlohmann::json toJson();

        static void fromJson(const nlohmann::json &data);

      private:
        std::vector<uuids::uuid> m_inputSlots;
        std::vector<uuids::uuid> m_outputSlots;

        int m_inputCount = 0;
        const std::shared_ptr<JComponentData> m_data;

        void onLeftClick(const glm::vec2 &pos);
        void onRightClick(const glm::vec2 &pos);

      private:
        void drawBackground(const glm::vec4 &borderRadiusPx, float rPx, float headerHeight, const glm::vec2 &gateSize);
    };
} // namespace Bess::Simulator::Components
