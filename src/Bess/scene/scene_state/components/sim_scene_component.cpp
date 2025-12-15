#include "scene/scene_state/components/sim_scene_component.h"

namespace Bess::Canvas {
    SlotSceneComponent::SlotSceneComponent(UUID uuid, UUID parentId)
        : SceneComponent(uuid), m_parentComponentId(parentId) {
        m_type = SceneComponentType::slot;
    }

    SlotSceneComponent::SlotSceneComponent(UUID uuid, const Transform &transform)
        : SceneComponent(uuid, transform) {
        m_type = SceneComponentType::slot;
    }

    SimulationSceneComponent::SimulationSceneComponent(UUID uuid) : SceneComponent(uuid) {
        m_type = SceneComponentType::simulation;
        initDragBehaviour();
    }

    SimulationSceneComponent::SimulationSceneComponent(UUID uuid, const Transform &transform)
        : SceneComponent(uuid, transform) {
        m_type = SceneComponentType::simulation;
        initDragBehaviour();
    }

    void SimulationSceneComponent::createIOSlots(size_t inputCount, size_t outputCount) {
        auto inpSlots = std::vector<SlotSceneComponent>(inputCount);
        auto outSlots = std::vector<SlotSceneComponent>(outputCount);

        // TODO: set proper positions based on index

        for (auto &slot : inpSlots) {
            slot.setParentComponentId(m_uuid);
            m_inputSlots[slot.getUuid()] = slot;
        }

        for (auto &slot : inpSlots) {
            slot.setParentComponentId(m_uuid);
            m_outputSlots[slot.getUuid()] = slot;
        }
    }

    void SimulationSceneComponent::onMouseHovered(const glm::vec2 &mousePos) {
        BESS_TRACE("[SimulationSceneComponent] {}: onMouseHovered at pos ({}, {})",
                   m_name, mousePos.x, mousePos.y);
    }

    void SimulationSceneComponent::onTransformChanged() {
        // // Update slot positions
        // for (auto &[uuid, slot] : m_inputSlots) {
        //     slot.setPosition(m_transform.position + glm::vec3(-30.f, 20.f * (float)slot.getSceneId(), 0.f));
        // }
        //
        // for (auto &[uuid, slot] : m_outputSlots) {
        //     slot.setPosition(m_transform.position + glm::vec3(130.f, 20.f * (float)slot.getSceneId(), 0.f));
        // }
    }

    void SimulationSceneComponent::draw(std::shared_ptr<Renderer::MaterialRenderer> renderer) {

        renderer->drawText(m_name, m_transform.position, 14.f, glm::vec4(1.f), 0);

        renderer->drawQuad(
            m_transform.position,
            glm::vec2(100.f, 100.f),
            m_isSelected
                ? glm::vec4(1.f, 0.f, 1.f, 1.f)
                : glm::vec4(1.f, 0.f, 0.f, 1.f),
            (uint64_t)m_uuid,
            {
                .borderColor = m_style.borderColor,
                .borderRadius = m_style.borderRadius,
                .borderSize = m_style.borderSize,
            });

        // Draw input slots
        for (auto &[uuid, slot] : m_inputSlots) {
            slot.draw(renderer);
        }

        // Draw output slots
        for (auto &[uuid, slot] : m_outputSlots) {
            slot.draw(renderer);
        }
    }
} // namespace Bess::Canvas
