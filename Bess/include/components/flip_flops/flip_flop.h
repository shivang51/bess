#pragma once

#include "../component.h"
#include "common/helpers.h"
#include "components/slot.h"
#include "components_manager/components_manager.h"
#include "json.hpp"
#include "uuid.h"

namespace Bess::Simulator::Components {

    class FlipFlop : public Component {
      public:
        FlipFlop(const uuids::uuid &uid, int renderId, glm::vec3 position, std::vector<uuids::uuid> inputSlots);
        FlipFlop(const uuids::uuid &uid, int renderId, const glm::vec3 &position, const std::vector<uuids::uuid> &inputSlots, const std::string &name, const std::vector<uuids::uuid> &outputSlots, const uuids::uuid &clockSlot);

        FlipFlop() = default;
        ~FlipFlop() = default;

        void render() override;

        void update() override;

        void drawBackground(const glm::vec4 &borderThicknessPx, float rPx, float headerHeight, const glm::vec2 &gateSize);

        template <typename T>
        void generate(int inputCount, const glm::vec3 &pos) {
            auto renderId = ComponentsManager::getNextRenderId();
            auto pId = Common::Helpers::uuidGenerator.getUUID();

            std::vector<uuids::uuid> inputSlots;
            for (int i = 0; i < inputCount; i++) {
                auto uid = Common::Helpers::uuidGenerator.getUUID();
                inputSlots.push_back(uid);
                auto renderId = ComponentsManager::getNextRenderId();
                ComponentsManager::components[uid] = std::make_shared<Components::Slot>(uid, pId, renderId, ComponentType::inputSlot);
                ComponentsManager::addCompIdToRId(renderId, uid);
                ComponentsManager::addRenderIdToCId(renderId, uid);
            }

            auto pos_ = pos;
            pos_.z = ComponentsManager::getNextZPos();

            ComponentsManager::components[pId] = std::make_shared<T>(pId, renderId, pos_, inputSlots);
            ComponentsManager::renderComponenets.emplace_back(pId);
            ComponentsManager::addCompIdToRId(renderId, pId);
            ComponentsManager::addRenderIdToCId(renderId, pId);
        }

        void generate(const glm::vec3 &pos = {0.f, 0.f, 0.f}) override;

        void deleteComponent() override;

        void drawProperties() override = 0;

        void simulate() override = 0;

        static void fromJson(const nlohmann::json &data);

        nlohmann::json toJson();

      protected:
        std::vector<uuids::uuid> m_inputSlots;
        std::vector<uuids::uuid> m_outputSlots;
        uuids::uuid m_clockSlot;
    };
} // namespace Bess::Simulator::Components
