#include "scene/components/components.h"
#include "simulation_engine.h"

namespace Bess::Canvas::Components {
    glm::mat4 TransformComponent::getTransform() const {
        auto transform = glm::translate(glm::mat4(1), position);
        transform = glm::rotate(transform, angle, {0.f, 0.f, 1.f});
        transform = glm::scale(transform, glm::vec3(scale, 1.f));
        return transform;
    }

    bool SimulationInputComponent::updateClock(const UUID &uuid) const {
        return SimEngine::SimulationEngine::instance().updateClock(uuid, clockBhaviour, frequency, frequencyUnit);
    }
} // namespace Bess::Canvas::Components
