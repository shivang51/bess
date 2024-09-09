#pragma once

#include "flip_flop.h"

namespace Bess::Simulator::Components {
    class DFlipFlop : public FlipFlop {
      public:
        static constexpr char name[] = "D Flip Flop";
        DFlipFlop(const uuids::uuid &uid, int renderId, glm::vec3 position, std::vector<uuids::uuid> inputSlots);
        DFlipFlop() = default;

        void update() override;

        void generate(const glm::vec3 &pos = {0.f, 0.f, 0.f}) override;

        void drawProperties() override;

        void simulate() override;
    };
} // namespace Bess::Simulator::Components
