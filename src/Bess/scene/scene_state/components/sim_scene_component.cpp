#include "scene/scene_state/components/sim_scene_component.h"
#include "bess_uuid.h"
#include "scene/scene_state/components/input_scene_component.h"
#include "scene/scene_state/components/scene_component.h"
#include "scene/scene_state/components/slot_scene_component.h"
#include "scene/scene_state/components/styles/comp_style.h"
#include "scene/scene_state/components/styles/sim_comp_style.h"
#include "scene/scene_state/scene_state.h"
#include "settings/viewport_theme.h"
#include "simulation_engine.h"

#include <algorithm>

namespace Bess::Canvas {
    constexpr float SNAP_AMOUNT = 5.f;

    SimulationSceneComponent::SimulationSceneComponent() {
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

        DrawHookOnDrawResult drawHookResult{.drawChildren = true, .drawOriginal = true};
        if (m_drawHook && m_drawHook->isDrawEnabled()) {
            const auto &compState = SimEngine::SimulationEngine::instance().getComponentState(m_simEngineId);
            drawHookResult = m_drawHook->onDraw(m_transform, pickingId, compState, materialRenderer, pathRenderer);

            if (drawHookResult.sizeChanged) {
                setScale(drawHookResult.newSize);
                calculateSchematicScale(state);
                resetSlotPositions(state);
            }
        }

        if (drawHookResult.drawOriginal) {
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

            const auto textPos = glm::vec3(m_transform.position.x - (m_transform.scale.x / 2.f) + Styles::componentStyles.paddingX,
                                           headerPos.y + Styles::simCompStyles.paddingY,
                                           m_transform.position.z + 0.0005f);
            // component name
            materialRenderer->drawText(m_name,
                                       textPos,
                                       Styles::simCompStyles.headerFontSize,
                                       ViewportTheme::colors.text,
                                       pickingId,
                                       m_transform.angle);
        }

        if (drawHookResult.drawChildren) {
            // slots
            for (const auto &childId : m_childComponents) {
                auto child = state.getComponentByUuid(childId);
                child->draw(state, materialRenderer, pathRenderer);
            }
        }
    }

    void SimulationSceneComponent::drawSchematic(SceneState &state,
                                                 std::shared_ptr<Renderer::MaterialRenderer> materialRenderer,
                                                 std::shared_ptr<Renderer2D::Vulkan::PathRenderer> pathRenderer) {

        if (m_isFirstSchematicDraw) {
            onFirstSchematicDraw(state, materialRenderer, pathRenderer);
        }
        const auto &id = PickingId{m_runtimeId, 0};

        if (m_drawHook && m_drawHook->isSchematicDrawEnabled()) {
            auto newScale = m_drawHook->onSchematicDraw(m_schematicTransform, id, materialRenderer, pathRenderer);
            const auto &prevScale = m_schematicTransform.scale;
            if (newScale.x != prevScale.x || newScale.y != prevScale.y) {
                BESS_TRACE("Schematic scale changed from draw hook");
                m_schematicTransform.scale = newScale;
                resetSchematicPinsPositions(state);
            }
        } else {
            float x = m_transform.position.x - (m_schematicTransform.scale.x / 2.f);
            float y = m_transform.position.y - (m_schematicTransform.scale.y / 2.f);
            float x1 = x + m_schematicTransform.scale.x;
            float y1 = y + m_schematicTransform.scale.y;
            const glm::vec3 &pos = getAbsolutePosition(state);
            float nodeWeight = Styles::compSchematicStyles.strokeSize;
            const auto &textColor = ViewportTheme::schematicViewColors.text;
            const auto &fillColor = ViewportTheme::schematicViewColors.componentFill;
            const auto &strokeColor = ViewportTheme::schematicViewColors.componentStroke;
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
        }

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
        inputPositions.reserve(inputCount);
        std::vector<glm::vec3> outputPositions;
        outputPositions.reserve(outputCount);

        const auto slotRowSize = Styles::SIM_COMP_SLOT_ROW_SIZE;

        for (size_t i = 0; i < inputCount; i++) {
            auto posX = -(pScale.x / 2.f) + Styles::SIM_COMP_SLOT_DX;
            float posY = -(pScale.y / 2.f) + (slotRowSize * (float)i) + (slotRowSize / 2.f);
            posY += Styles::SIM_COMP_SLOT_START_Y;
            glm::vec2 pos = glm::round(glm::vec2(posX, posY) / SNAP_AMOUNT) * SNAP_AMOUNT;
            inputPositions.emplace_back(pos.x, pos.y, 0.0005f);
        }

        for (size_t i = 0; i < outputCount; i++) {
            auto posX = (pScale.x / 2.f) - Styles::SIM_COMP_SLOT_DX;
            float posY = -(pScale.y / 2.f) + (slotRowSize * (float)i) + (slotRowSize / 2.f);
            posY += Styles::SIM_COMP_SLOT_START_Y;
            glm::vec2 pos = glm::round(glm::vec2(posX, posY) / SNAP_AMOUNT) * SNAP_AMOUNT;
            outputPositions.emplace_back(pos.x, pos.y, 0.0005f);
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

        width = glm::round(width / SNAP_AMOUNT) * SNAP_AMOUNT;
        height = glm::round(height / SNAP_AMOUNT) * SNAP_AMOUNT;

        return {width, height};
    }

    void SimulationSceneComponent::resetSlotPositions(SceneState &state) {
        const auto [inpPositions, outPositions] =
            calculateSlotPositions(m_inputSlots.size(), m_outputSlots.size());

        for (size_t i = 0; i < inpPositions.size(); i++) {
            const auto slotComp = state.getComponentByUuid<SlotSceneComponent>(m_inputSlots[i]);
            slotComp->setPosition(inpPositions[i]);
        }

        for (size_t i = 0; i < outPositions.size(); i++) {
            const auto slotComp = state.getComponentByUuid<SlotSceneComponent>(m_outputSlots[i]);
            slotComp->setPosition(outPositions[i]);
        }
    }

    void SimulationSceneComponent::resetSchematicPinsPositions(SceneState &state) {
        // Schematic diagram pin positions
        // We will ignore resize slots for schematic view positioning
        // Resize slots will be hidden in schematic view.
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

        const float inpStartX = -m_schematicTransform.scale.x / 2.f;
        const float inpOffsetY = (m_schematicTransform.scale.y / ((float)inpCount + 1.f));
        const float outStartX = m_schematicTransform.scale.x / 2.f;
        const float outOffsetY = (m_schematicTransform.scale.y / ((float)outCount + 1.f));
        const float startY = -(m_schematicTransform.scale.y / 2.f);

        for (size_t i = 0; i < inpCount; i++) {
            const auto slotComp = state.getComponentByUuid<SlotSceneComponent>(m_inputSlots[i]);
            auto pos = glm::vec2(inpStartX, startY + (inpOffsetY * (float)(i + 1)));
            pos.y = glm::round(pos.y / SNAP_AMOUNT) * SNAP_AMOUNT;
            slotComp->setSchematicPos(glm::vec3(pos, -0.0005f));
        }

        for (size_t i = 0; i < outCount; i++) {
            const auto slotComp = state.getComponentByUuid<SlotSceneComponent>(m_outputSlots[i]);
            auto pos = glm::vec2(outStartX, startY + (outOffsetY * (float)(i + 1)));
            pos.y = glm::round(pos.y / SNAP_AMOUNT) * SNAP_AMOUNT;
            slotComp->setSchematicPos(glm::vec3(pos, -0.0005));
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
        resetSchematicPinsPositions(sceneState);
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
        const auto ids = SceneComponent::cleanup(state, caller);
        auto &simEngine = SimEngine::SimulationEngine::instance();
        simEngine.deleteComponent(m_simEngineId);
        if (m_drawHook) {
            m_drawHook.reset();
        }
        return ids;
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

        m_schematicTransform.scale = {width, height};
        m_schematicTransform.scale = glm::round(m_schematicTransform.scale / SNAP_AMOUNT) * SNAP_AMOUNT;
    }

    std::shared_ptr<SimulationSceneComponent> SimulationSceneComponent::createNewAndRegister(SceneState &sceneState, UUID simEngineId) {
        auto &simEngine = SimEngine::SimulationEngine::instance();
        const auto &simComp = simEngine.getDigitalComponent(simEngineId);
        const auto &compDef = simComp->definition;
        const bool isInput = compDef->getBehaviorType() == SimEngine::ComponentBehaviorType::input;
        const bool isOutput = compDef->getBehaviorType() == SimEngine::ComponentBehaviorType::output;

        const UUID uuid;
        std::shared_ptr<SimulationSceneComponent> sceneComp;
        if (isInput) {
            sceneComp = std::make_shared<InputSceneComponent>();
        } else {
            sceneComp = std::make_shared<SimulationSceneComponent>();
        }

        // setting the name before adding to scene state, so that event listeners can access it
        sceneComp->setName(compDef->getName());
        sceneComp->setSimEngineId(simEngineId);

        sceneState.addComponent<SimulationSceneComponent>(sceneComp);

        // style
        auto &style = sceneComp->getStyle();

        style.color = ViewportTheme::colors.componentBG;
        style.borderRadius = glm::vec4(6.f);
        style.headerColor = ViewportTheme::getCompHeaderColor(compDef->getGroupName());
        style.borderColor = ViewportTheme::colors.componentBorder;
        style.borderSize = glm::vec4(1.f);
        style.color = ViewportTheme::colors.componentBG;

        const auto &inpDetails = compDef->getInputSlotsInfo();
        const auto &outDetails = compDef->getOutputSlotsInfo();

        int inSlotIdx = 0, outSlotIdx = 0;
        char inpCh = 'A', outCh = 'a';

        const auto slots = sceneComp->createIOSlots(compDef->getInputSlotsInfo().count,
                                                    compDef->getOutputSlotsInfo().count);

        for (const auto &slot : slots) {
            if (slot->getSlotType() == SlotType::digitalInput) {
                if (inpDetails.names.size() > inSlotIdx)
                    slot->setName(inpDetails.names[inSlotIdx++]);
                else
                    slot->setName(std::string(1, inpCh++));
            } else {
                if (outDetails.names.size() > outSlotIdx)
                    slot->setName(outDetails.names[outSlotIdx++]);
                else
                    slot->setName(std::string(1, outCh++));
            }
            sceneState.addComponent<SlotSceneComponent>(slot);
            sceneState.attachChild(sceneComp->getUuid(), slot->getUuid());
        }

        if (inpDetails.isResizeable) {
            auto slot = std::make_shared<SlotSceneComponent>();
            slot->setSlotType(SlotType::inputsResize);
            slot->setIndex(-1); // assign -1 for resize slots
            sceneComp->addInputSlot(slot->getUuid(), false);
            sceneState.addComponent<SlotSceneComponent>(slot);
            sceneState.attachChild(sceneComp->getUuid(), slot->getUuid());
        }

        if (outDetails.isResizeable) {
            auto slot = std::make_shared<SlotSceneComponent>();
            slot->setSlotType(SlotType::outputsResize);
            slot->setIndex(-1); // assign -1 for resize slots
            sceneComp->addOutputSlot(slot->getUuid(), false);
            sceneState.addComponent<SlotSceneComponent>(slot);
            sceneState.attachChild(sceneComp->getUuid(), slot->getUuid());
        }

        return sceneComp;
    }

    void SimulationSceneComponent::removeChildComponent(const UUID &uuid) {
        BESS_INFO("[SimulationSceneComponent] Removing slot component {}", (uint64_t)uuid);
        SceneComponent::removeChildComponent(uuid);
        m_inputSlots.erase(std::ranges::remove(m_inputSlots, uuid).begin(),
                           m_inputSlots.end());

        m_outputSlots.erase(std::ranges::remove(m_outputSlots, uuid).begin(),
                            m_outputSlots.end());
    }

    void SimulationSceneComponent::onTransformChanged() {
        m_schematicTransform.position = m_transform.position;
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

        toJsonValue(component.getSchematicTransform(), j["schematicTransform"]);
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

        if (j.isMember("schematicTransform")) {
            Canvas::Transform schematicTransform;
            fromJsonValue(j["schematicTransform"], schematicTransform);
            component.setSchematicTransform(schematicTransform);
        }
    }

} // namespace Bess::JsonConvert
