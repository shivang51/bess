#include "scene/scene_state/components/sim_scene_component.h"
#include "scene/scene_state/components/styles/comp_style.h"
#include "scene/scene_state/components/styles/sim_comp_style.h"
#include <cstdint>

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

        const auto [inpPositions, outPositions] = calculateSlotPositions(inputCount, outputCount);

        const auto &pos = m_transform.position;

        for (size_t i = 0; i < inputCount; i++) {
            auto &slot = inpSlots[i];
            slot.setParentComponentId(m_uuid);
            slot.setPosition(inpPositions[i]);
            slot.setParentTransform(m_transform);
            slot.setSlotType(SlotType::digitalInput);
            m_inputSlots.emplace_back(slot);
        }

        for (size_t i = 0; i < outputCount; i++) {
            auto &slot = outSlots[i];
            slot.setParentComponentId(m_uuid);
            slot.setPosition(outPositions[i]);
            slot.setParentTransform(m_transform);
            slot.setSlotType(SlotType::digitalOutput);
            m_outputSlots.emplace_back(slot);
        }
    }

    void SimulationSceneComponent::onMouseHovered(const glm::vec2 &mousePos) {
        BESS_TRACE("[SimulationSceneComponent] {}: onMouseHovered at pos ({}, {})",
                   m_name, mousePos.x, mousePos.y);
    }

    void SimulationSceneComponent::onTransformChanged() {
        for (auto &slot : m_inputSlots) {
            slot.setParentTransform(m_transform);
        }

        for (auto &slot : m_outputSlots) {
            slot.setParentTransform(m_transform);
        }
    }

    void SlotSceneComponent::draw(std::shared_ptr<Renderer::MaterialRenderer> renderer) {
        const auto pos = m_parentTransform.position + m_transform.position;
        auto bg = ViewportTheme::colors.stateLow;

        auto border = bg;
        bg.a = 0.1f;

        const float ir = Styles::simCompStyles.slotRadius - Styles::simCompStyles.slotBorderSize;
        const float r = Styles::simCompStyles.slotRadius;
        renderer->drawCircle(pos, r, border, m_uuid, ir);
        renderer->drawCircle(pos, ir - 1.f, bg, m_uuid);

        const float labeldx = Styles::simCompStyles.slotMargin + (Styles::simCompStyles.slotRadius * 2.f);
        float labelX = pos.x;
        if (m_slotType == SlotType::digitalInput) {
            labelX += labeldx;
        } else {
            labelX -= labeldx;
        }
        float dY = componentStyles.slotRadius - (std::abs((componentStyles.slotRadius * 2.f) - componentStyles.slotLabelSize) / 2.f);
        renderer->drawText(m_name,
                           {labelX, pos.y + dY, pos.z},
                           componentStyles.slotLabelSize,
                           ViewportTheme::colors.text,
                           m_parentComponentId,
                           m_parentTransform.angle);
    }

    void SimulationSceneComponent::draw(std::shared_ptr<Renderer::MaterialRenderer> renderer) {
        if (m_isFirstDraw) {
            onFirstDraw(renderer);
        }

        // background
        Renderer::QuadRenderProperties props;
        props.angle = m_transform.angle;
        props.borderRadius = m_style.borderRadius;
        props.borderSize = m_style.borderSize;
        props.borderColor = m_style.borderColor;
        props.isMica = true;
        props.hasShadow = true;

        renderer->drawQuad(m_transform.position,
                           m_transform.scale,
                           m_style.color,
                           m_uuid,
                           props);

        props = {};
        props.angle = m_transform.angle;
        props.borderSize = glm::vec4(0.f);
        props.borderRadius = glm::vec4(0,
                                       0,
                                       m_style.borderRadius.x - m_style.borderSize.x,
                                       m_style.borderRadius.y - m_style.borderSize.y);
        props.isMica = true;

        // given based on header presence
        glm::vec3 textPos;

        // header
        if (!isCompHeaderLess()) {
            const float headerHeight = Styles::componentStyles.headerHeight;
            const auto headerPos = glm::vec3(m_transform.position.x,
                                             m_transform.position.y - (m_transform.scale.y / 2.f) + (headerHeight / 2.f),
                                             m_transform.position.z + 0.0004f);
            renderer->drawQuad(headerPos,
                               glm::vec2(m_transform.scale.x - m_style.borderSize.w - m_style.borderSize.y,
                                         headerHeight - m_style.borderSize.x - m_style.borderSize.z),
                               m_style.headerColor,
                               m_uuid,
                               props);

            textPos = glm::vec3(m_transform.position.x - (m_transform.scale.x / 2.f) + componentStyles.paddingX,
                                headerPos.y + Styles::simCompStyles.paddingY,
                                m_transform.position.z + 0.0005f);
        } else {
            float labelWidth = renderer->getTextRenderSize(m_name, Styles::simCompStyles.headerFontSize).x;
            textPos = glm::vec3(m_transform.position.x,
                                m_transform.position.y + (Styles::simCompStyles.headerFontSize / 2.f) - 1.f,
                                m_transform.position.z + 0.0005f);

            if (m_inputSlots.empty()) {
                textPos.x -= (m_transform.scale.x / 2.f) - Styles::simCompStyles.paddingX;
            } else if (m_outputSlots.empty()) {
                textPos.x += (m_transform.scale.x / 2.f) - labelWidth - Styles::simCompStyles.paddingX;
            } else {
                textPos.x -= labelWidth / 2.f;
            }
        }

        // component name
        renderer->drawText(m_name,
                           textPos,
                           Styles::simCompStyles.headerFontSize,
                           ViewportTheme::colors.text, m_uuid, m_transform.angle);

        // slots
        for (auto &slot : m_inputSlots) {
            slot.draw(renderer);
        }

        for (auto &slot : m_outputSlots) {
            slot.draw(renderer);
        }
    }

    std::pair<std::vector<glm::vec3>, std::vector<glm::vec3>>
    SimulationSceneComponent::calculateSlotPositions(size_t inputCount, size_t outputCount) const {
        const auto pScale = m_transform.scale;
        const bool isHeaderLess = isCompHeaderLess();

        std::vector<glm::vec3> inputPositions;
        std::vector<glm::vec3> outputPositions;

        const auto slotRowSize = Styles::SIM_COMP_SLOT_ROW_SIZE;

        for (size_t i = 0; i < inputCount; i++) {
            auto posX = -(pScale.x / 2.f) + Styles::SIM_COMP_SLOT_DX;
            float posY = -(pScale.y / 2.f) + (slotRowSize * (float)i) + (slotRowSize / 2.f);
            if (!isHeaderLess)
                posY += Styles::SIM_COMP_SLOT_START_Y;
            inputPositions.emplace_back(posX, posY, 0.0005f);
        }

        for (size_t i = 0; i < outputCount; i++) {
            auto posX = (pScale.x / 2.f) - Styles::SIM_COMP_SLOT_DX;
            float posY = -(pScale.y / 2.f) + (slotRowSize * (float)i) + (slotRowSize / 2.f);
            if (!isHeaderLess)
                posY += Styles::SIM_COMP_SLOT_START_Y;
            outputPositions.emplace_back(posX, posY, 0.0005f);
        }

        return {inputPositions, outputPositions};
    }

    glm::vec2 SimulationSceneComponent::calculateScale(std::shared_ptr<Renderer::MaterialRenderer> materialRenderer) {
        const auto labelSize = materialRenderer->getTextRenderSize(m_name, Styles::simCompStyles.headerFontSize);
        float width = labelSize.x + (Styles::simCompStyles.paddingX * 2.f);
        size_t maxRows = std::max(m_inputSlots.size(), m_outputSlots.size());
        float height = ((float)maxRows * Styles::SIM_COMP_SLOT_ROW_SIZE);

        const bool isHeaderLess = isCompHeaderLess();
        if (!isHeaderLess) {
            width = std::max(width, 100.f);
            height += Styles::simCompStyles.headerHeight + Styles::simCompStyles.rowGap;
        } else { // header less component

            if (!m_inputSlots.empty()) {
                constexpr float labelXGapL = 2.f;
                width += Styles::SIM_COMP_SLOT_COLUMN_SIZE + labelXGapL;
            }

            if (!m_outputSlots.empty()) {
                constexpr float labelXGapR = 2.f;
                width += Styles::SIM_COMP_SLOT_COLUMN_SIZE + labelXGapR;
            }
        }

        return {width, height};
    }

    void SimulationSceneComponent::resetSlotPositions() {
        const auto [inpPositions, outPositions] =
            calculateSlotPositions(m_inputSlots.size(), m_outputSlots.size());

        for (size_t i = 0; i < inpPositions.size(); i++) {
            m_inputSlots[i].setPosition(inpPositions[i]);
        }

        for (size_t i = 0; i < outPositions.size(); i++) {
            m_outputSlots[i].setPosition(outPositions[i]);
        }
    }

    void SimulationSceneComponent::onFirstDraw(const std::shared_ptr<Renderer::MaterialRenderer> &materialRenderer) {
        setScale(calculateScale(materialRenderer));
        resetSlotPositions();
        m_isFirstDraw = false;
    }

    bool SimulationSceneComponent::isCompHeaderLess() const {
        size_t maxRows = std::max(m_inputSlots.size(), m_outputSlots.size());
        return maxRows < 2;
    }
} // namespace Bess::Canvas
