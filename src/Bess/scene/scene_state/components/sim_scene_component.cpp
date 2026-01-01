#include "scene/scene_state/components/sim_scene_component.h"
#include "scene/scene_state/components/scene_component.h"
#include "scene/scene_state/components/slot_scene_component.h"
#include "scene/scene_state/components/styles/comp_style.h"
#include "scene/scene_state/components/styles/sim_comp_style.h"
#include "settings/viewport_theme.h"
#include "simulation_engine.h"

namespace Bess::Canvas {
    SimulationSceneComponent::SimulationSceneComponent(UUID uuid) : SceneComponent(uuid) {
        initDragBehaviour();
    }

    SimulationSceneComponent::SimulationSceneComponent(UUID uuid, const Transform &transform)
        : SceneComponent(uuid, transform) {
        initDragBehaviour();
    }

    std::vector<std::shared_ptr<SlotSceneComponent>>
    SimulationSceneComponent::createIOSlots(size_t inputCount, size_t outputCount) {
        std::vector<std::shared_ptr<SlotSceneComponent>> slots;

        for (size_t i = 0; i < inputCount; i++) {
            auto slot = std::make_shared<SlotSceneComponent>();
            slot->setSlotType(SlotType::digitalInput);
            slot->setIndex(static_cast<int>(i));
            m_inputSlots.push_back(slot->getUuid());
            slots.push_back(slot);
        }

        for (size_t i = 0; i < outputCount; i++) {
            auto slot = std::make_shared<SlotSceneComponent>();
            slot->setSlotType(SlotType::digitalOutput);
            slot->setIndex(static_cast<int>(i));
            m_outputSlots.push_back(slot->getUuid());
            slots.push_back(slot);
        }

        return slots;
    }

    void SimulationSceneComponent::draw(SceneState &state,
                                        std::shared_ptr<Renderer::MaterialRenderer> materialRenderer,
                                        std::shared_ptr<Renderer2D::Vulkan::PathRenderer> pathRenderer) {
        if (m_isFirstDraw) {
            onFirstDraw(state, materialRenderer, pathRenderer);
        } else if (m_isScaleDirty) {
            setScale(calculateScale(materialRenderer));
            calculateSchematicScale(state);
            resetSlotPositions(state);
            m_isScaleDirty = false;
        }

        const auto pickingId = PickingId{m_runtimeId, 0};

        // background
        Renderer::QuadRenderProperties props;
        props.angle = m_transform.angle;
        props.borderRadius = m_style.borderRadius;
        props.borderSize = m_style.borderSize;
        props.borderColor = m_isSelected
                                ? ViewportTheme::colors.selectedComp
                                : m_style.borderColor;
        props.isMica = true;
        props.hasShadow = true;

        materialRenderer->drawQuad(m_transform.position,
                                   m_transform.scale,
                                   m_style.color,
                                   pickingId,
                                   props);

        props = {};
        props.angle = m_transform.angle;
        props.borderSize = glm::vec4(0.f);
        props.borderRadius = glm::vec4(0,
                                       0,
                                       m_style.borderRadius.x - m_style.borderSize.x,
                                       m_style.borderRadius.y - m_style.borderSize.y);
        props.isMica = true;

        // header
        const float headerHeight = Styles::componentStyles.headerHeight;
        const auto headerPos = glm::vec3(m_transform.position.x,
                                         m_transform.position.y - (m_transform.scale.y / 2.f) + (headerHeight / 2.f),
                                         m_transform.position.z + 0.0004f);
        materialRenderer->drawQuad(headerPos,
                                   glm::vec2(m_transform.scale.x - m_style.borderSize.w - m_style.borderSize.y,
                                             headerHeight - m_style.borderSize.x - m_style.borderSize.z),
                                   m_style.headerColor,
                                   pickingId,
                                   props);

        const auto textPos = glm::vec3(m_transform.position.x - (m_transform.scale.x / 2.f) + componentStyles.paddingX,
                                       headerPos.y + Styles::simCompStyles.paddingY,
                                       m_transform.position.z + 0.0005f);
        // component name
        materialRenderer->drawText(m_name,
                                   textPos,
                                   Styles::simCompStyles.headerFontSize,
                                   ViewportTheme::colors.text,
                                   pickingId,
                                   m_transform.angle);

        // slots
        for (const auto &childId : m_childComponents) {
            auto child = state.getComponentByUuid(childId);
            child->draw(state, materialRenderer, pathRenderer);
        }
    }

    void SimulationSceneComponent::drawSchematic(SceneState &state,
                                                 std::shared_ptr<Renderer::MaterialRenderer> materialRenderer,
                                                 std::shared_ptr<Renderer2D::Vulkan::PathRenderer> pathRenderer) {

        if (m_isFirstSchematicDraw) {
            onFirstSchematicDraw(state, materialRenderer, pathRenderer);
        }

        float x = m_transform.position.x - (m_schematicScale.x / 2.f);
        float y = m_transform.position.y - (m_schematicScale.y / 2.f);
        float x1 = x + m_schematicScale.x;
        float y1 = y + m_schematicScale.y;
        const glm::vec3 &pos = getAbsolutePosition(state);
        float nodeWeight = Styles::compSchematicStyles.strokeSize;
        const auto &textColor = ViewportTheme::schematicViewColors.text;
        const auto &fillColor = ViewportTheme::schematicViewColors.componentFill;
        const auto &strokeColor = ViewportTheme::schematicViewColors.componentStroke;
        const auto &id = PickingId{m_runtimeId, 0};
        pathRenderer->beginPathMode({x, y, pos.z}, nodeWeight, strokeColor, id);
        pathRenderer->pathLineTo({x1, y, pos.z}, nodeWeight, strokeColor, id);
        pathRenderer->pathLineTo({x1, y1, pos.z}, nodeWeight, strokeColor, id);
        pathRenderer->pathLineTo({x, y1, pos.z}, nodeWeight, strokeColor, id);
        pathRenderer->endPathMode(true, true, fillColor);

        const auto textSize = materialRenderer->getTextRenderSize(m_name,
                                                                  Styles::compSchematicStyles.nameFontSize);
        glm::vec3 textPos = {pos.x, y + ((y1 - y) / 2.f), pos.z + 0.0005f};
        textPos.x -= textSize.x / 2.f;
        textPos.y += Styles::simCompStyles.headerFontSize / 2.f;
        materialRenderer->drawText(m_name,
                                   textPos,
                                   Styles::compSchematicStyles.nameFontSize,
                                   textColor, id, 0.f);

        // slots
        for (const auto &childId : m_childComponents) {
            auto child = state.getComponentByUuid(childId);
            child->drawSchematic(state, materialRenderer, pathRenderer);
        }
    }

    std::pair<std::vector<glm::vec3>, std::vector<glm::vec3>>
    SimulationSceneComponent::calculateSlotPositions(size_t inputCount, size_t outputCount) const {
        const auto pScale = m_transform.scale;

        std::vector<glm::vec3> inputPositions;
        std::vector<glm::vec3> outputPositions;

        const auto slotRowSize = Styles::SIM_COMP_SLOT_ROW_SIZE;

        for (size_t i = 0; i < inputCount; i++) {
            auto posX = -(pScale.x / 2.f) + Styles::SIM_COMP_SLOT_DX;
            float posY = -(pScale.y / 2.f) + (slotRowSize * (float)i) + (slotRowSize / 2.f);
            posY += Styles::SIM_COMP_SLOT_START_Y;
            inputPositions.emplace_back(posX, posY, 0.0005f);
        }

        for (size_t i = 0; i < outputCount; i++) {
            auto posX = (pScale.x / 2.f) - Styles::SIM_COMP_SLOT_DX;
            float posY = -(pScale.y / 2.f) + (slotRowSize * (float)i) + (slotRowSize / 2.f);
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

        width = std::max(width, 100.f);
        height += Styles::simCompStyles.headerHeight + Styles::simCompStyles.rowGap;

        return {width, height};
    }

    void SimulationSceneComponent::resetSlotPositions(SceneState &state) {
        const auto [inpPositions, outPositions] =
            calculateSlotPositions(m_inputSlots.size(), m_outputSlots.size());

        // Schematic diagram pin positions
        // We will ignore resize slots for schematic view positioning
        // Resize slots will be hidden in schematic view.
        auto inpCount = inpPositions.size();
        auto outCount = outPositions.size();
        if (inpCount != 0 &&
            state.getComponentByUuid<SlotSceneComponent>(m_inputSlots.back())->isResizeSlot()) {
            inpCount -= 1;
        }

        if (outCount != 0 &&
            state.getComponentByUuid<SlotSceneComponent>(m_outputSlots.back())->isResizeSlot()) {
            outCount -= 1;
        }

        const float inpStartX = -m_schematicScale.x / 2.f;
        const float inpOffsetY = (m_schematicScale.y / ((float)inpCount + 1.f));
        const float outStartX = m_schematicScale.x / 2.f;
        const float outOffsetY = (m_schematicScale.y / ((float)outCount + 1.f));
        const float startY = -(m_schematicScale.y / 2.f);

        for (size_t i = 0; i < inpPositions.size(); i++) {
            const auto slotComp = state.getComponentByUuid<SlotSceneComponent>(m_inputSlots[i]);
            slotComp->setPosition(inpPositions[i]);
            slotComp->setSchematicPos(glm::vec3(inpStartX,
                                                startY + (inpOffsetY * (float)(i + 1)),
                                                inpPositions[i].z));
        }

        for (size_t i = 0; i < outPositions.size(); i++) {
            const auto slotComp = state.getComponentByUuid<SlotSceneComponent>(m_outputSlots[i]);
            slotComp->setPosition(outPositions[i]);
            slotComp->setSchematicPos(glm::vec3(outStartX,
                                                startY + (outOffsetY * (float)(i + 1)),
                                                outPositions[i].z));
        }
    }

    void SimulationSceneComponent::onFirstDraw(SceneState &sceneState,
                                               std::shared_ptr<Renderer::MaterialRenderer> materialRenderer,
                                               std::shared_ptr<PathRenderer> /*unused*/) {
        setScale(calculateScale(materialRenderer));
        resetSlotPositions(sceneState);
        m_isFirstDraw = false;
    }

    void SimulationSceneComponent::onFirstSchematicDraw(SceneState &sceneState,
                                                        std::shared_ptr<Renderer::MaterialRenderer> materialRenderer,
                                                        std::shared_ptr<PathRenderer> /*unused*/) {
        if (m_isFirstDraw) {
            setScale(calculateScale(materialRenderer));
        }

        calculateSchematicScale(sceneState);
        resetSlotPositions(sceneState);
        m_isFirstSchematicDraw = false;
    }

    size_t SimulationSceneComponent::getInputSlotsCount() const {
        return m_inputSlots.size();
    }

    size_t SimulationSceneComponent::getOutputSlotsCount() const {
        return m_outputSlots.size();
    }

    void SimulationSceneComponent::addOutputSlot(UUID slotId, bool isLastResizeable) {
        if (isLastResizeable) {
            m_outputSlots.insert(m_outputSlots.end() - 1, slotId);
        } else {
            m_outputSlots.emplace_back(slotId);
        }
    }

    void SimulationSceneComponent::addInputSlot(UUID slotId, bool isLastResizeable) {
        if (isLastResizeable) {
            m_inputSlots.insert(m_inputSlots.end() - 1, slotId);
        } else {
            m_inputSlots.emplace_back(slotId);
        }
    }

    void SimulationSceneComponent::setScaleDirty() {
        m_isScaleDirty = true;
    }

    std::vector<UUID> SimulationSceneComponent::cleanup(SceneState &state, UUID caller) {
        auto &simEngine = SimEngine::SimulationEngine::instance();
        simEngine.deleteComponent(m_simEngineId);
        return SceneComponent::cleanup(state, caller);
    }

    void SimulationSceneComponent::calculateSchematicScale(SceneState &state) {
        auto inpCount = m_inputSlots.size();
        auto outCount = m_outputSlots.size();
        if (inpCount != 0 &&
            state.getComponentByUuid<SlotSceneComponent>(m_inputSlots.back())->isResizeSlot()) {
            inpCount -= 1;
        }

        if (outCount != 0 &&
            state.getComponentByUuid<SlotSceneComponent>(m_outputSlots.back())->isResizeSlot()) {
            outCount -= 1;
        }

        const size_t maxRows = std::max(inpCount, outCount);
        const float height = ((float)maxRows * Styles::SCHEMATIC_VIEW_PIN_ROW_SIZE);

        float width = m_transform.scale.x; // keep the same width as normal view

        m_schematicScale = {width, height};
    }
} // namespace Bess::Canvas

namespace Bess::JsonConvert {
    void toJsonValue(const Bess::Canvas::SimulationSceneComponent &component, Json::Value &j) {
        toJsonValue(component.cast<Canvas::SceneComponent>(), j);
        toJsonValue(component.getSimEngineId(), j["simEngineId"]);
        toJsonValue(component.getNetId(), j["netId"]);

        for (size_t i = 0; i < component.getInputSlotsCount(); i++) {
            j["inputSlots"].append(Json::Value());
            toJsonValue(component.getInputSlots()[i],
                        j["inputSlots"][static_cast<int>(i)]);
        }

        for (size_t i = 0; i < component.getOutputSlotsCount(); i++) {
            j["outputSlots"].append(Json::Value());
            toJsonValue(component.getOutputSlots()[i],
                        j["outputSlots"][static_cast<int>(i)]);
        }
    }

    void fromJsonValue(const Json::Value &j, Bess::Canvas::SimulationSceneComponent &component) {

        fromJsonValue(j, (Canvas::SceneComponent &)component);

        if (j.isMember("simEngineId")) {
            UUID simEngineId;
            fromJsonValue(j["simEngineId"], simEngineId);
            component.setSimEngineId(simEngineId);
        }

        if (j.isMember("netId")) {
            UUID netId;
            fromJsonValue(j["netId"], netId);
            component.setNetId(netId);
        }

        if (j.isMember("inputSlots") && j["inputSlots"].isArray()) {
            component.getInputSlots().clear();
            for (const auto &slotJson : j["inputSlots"]) {
                UUID slotId;
                fromJsonValue(slotJson, slotId);
                component.getInputSlots().push_back(slotId);
            }
        }

        if (j.isMember("outputSlots") && j["outputSlots"].isArray()) {
            component.getOutputSlots().clear();
            for (const auto &slotJson : j["outputSlots"]) {
                UUID slotId;
                fromJsonValue(slotJson, slotId);
                component.getOutputSlots().push_back(slotId);
            }
        }
    }

} // namespace Bess::JsonConvert
