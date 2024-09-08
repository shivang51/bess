#pragma once

#include "flip_flop.h"

namespace Bess::Simulator::Components {
    class JKFlipFlop : public FlipFlop {
      public:
        static constexpr std::string name = "JK Flip Flop";
        JKFlipFlop(const uuids::uuid &uid, int renderId, glm::vec3 position, std::vector<uuids::uuid> inputSlots);
        JKFlipFlop(const uuids::uuid &uid, int renderId, glm::vec3 position, std::vector<uuids::uuid> inputSlots, std::vector<uuids::uuid> outputSlots, uuids::uuid clockSlot);
        JKFlipFlop() = default;
        ~JKFlipFlop() = default;

        void update() override;

        void generate(const glm::vec3 &pos = {0.f, 0.f, 0.f}) override;

        void drawProperties() override;

        void simulate() override;
    };
} // namespace Bess::Simulator::Components
