#include "scene/artist/nodes_artist.h"
#include "component_catalog.h"
#include "component_types/component_types.h"
#include "entt/entity/fwd.hpp"
#include "ext/vector_float3.hpp"
#include "scene/components/components.h"
#include "scene/scene.h"
#include "settings/viewport_theme.h"
#include "simulation_engine.h"
#include "types.h"
#include <cstdint>
#include <string>

namespace Bess::Canvas {

    NodesArtist::NodesArtist(std::shared_ptr<Scene> scene) : BaseArtist(scene) {
    }

    void NodesArtist::drawSimEntity(
        entt::entity entity,
        const Components::TagComponent &tagComp,
        const Components::TransformComponent &transform,
        const Components::SpriteComponent &spriteComp,
        const Components::SimulationComponent &simComponent) {

        auto &registry = m_sceneRef->getEnttRegistry();

        if (tagComp.type.simCompType == SimEngine::ComponentType::SEVEN_SEG_DISPLAY) {
            drawSevenSegDisplay(entity, tagComp, transform, spriteComp, simComponent);
            return;
        }

        if (isHeaderLessComp(simComponent)) {
            drawHeaderLessComp(entity, tagComp, transform, spriteComp, simComponent);
            return;
        }

        const auto &pos = transform.position;
        const auto &rotation = transform.angle;
        const auto &scale = transform.scale;

        float headerHeight = componentStyles.headerHeight;
        auto headerPos = glm::vec3(pos.x, pos.y - scale.y / 2.f + headerHeight / 2.f, pos.z);

        bool isSelected = registry.any_of<Components::SelectedComponent>(entity);
        auto border = isSelected ? ViewportTheme::colors.selectedComp : spriteComp.borderColor;

        uint64_t id = (uint64_t)entity;

        glm::vec3 textPos = glm::vec3(pos.x - scale.x / 2.f + componentStyles.paddingX,
                                      headerPos.y + componentStyles.paddingY,
                                      pos.z + 0.0005f);

        QuadRenderProperties props;
        props.angle = rotation;
        props.borderRadius = spriteComp.borderRadius;
        props.borderSize = spriteComp.borderSize;
        props.borderColor = border;
        props.isMica = true;
        props.hasShadow = true;

        m_sceneRef->getViewport()->quad(pos, glm::vec2(scale), spriteComp.color, id, props);

        props = {};
        props.angle = rotation;
        props.borderSize = glm::vec4(0.f);
        props.borderRadius = glm::vec4(0, 0, spriteComp.borderRadius.x - spriteComp.borderSize.x, spriteComp.borderRadius.y - spriteComp.borderSize.y);
        props.isMica = true;

        m_sceneRef->getViewport()->quad(headerPos,
                                        glm::vec2(scale.x - spriteComp.borderSize.w - spriteComp.borderSize.y, headerHeight - spriteComp.borderSize.x - spriteComp.borderSize.z),
                                        spriteComp.headerColor,
                                        id,
                                        props);

        m_sceneRef->getViewport()->msdfText(tagComp.name, textPos, componentStyles.headerFontSize, ViewportTheme::colors.text, id, rotation);

        drawSlots(entity, simComponent, transform);
    }

    void NodesArtist::drawHeaderLessComp(entt::entity entity,
                                         const Components::TagComponent &tagComp,
                                         const Components::TransformComponent &transform,
                                         const Components::SpriteComponent &spriteComp,
                                         const Components::SimulationComponent &simComp) {
        auto &registry = m_sceneRef->getEnttRegistry();

        auto labelSize = m_sceneRef->getViewport()->getMSDFTextRenderSize(tagComp.name, componentStyles.headerFontSize);

        uint64_t id = (uint64_t)entity;
        const auto &pos = transform.position;
        const auto &rotation = transform.angle;
        const auto &scale = transform.scale;

        float labelXGapL = 0.f, labelXGapR = 0.f;
        float labelLOffset = componentStyles.paddingX;

        if (!simComp.inputSlots.empty()) {
            labelXGapL = 2.f;
            labelLOffset += SLOT_COLUMN_SIZE + labelXGapL;
        }

        if (!simComp.outputSlots.empty()) {
            labelXGapR = 2.f;
        }

        bool isSelected = registry.any_of<Components::SelectedComponent>(entity);
        auto border = isSelected ? ViewportTheme::colors.selectedComp : spriteComp.borderColor;

        QuadRenderProperties props;
        props.borderRadius = spriteComp.borderRadius;
        props.borderColor = border;
        props.borderSize = spriteComp.borderSize;
        props.isMica = true;
        m_sceneRef->getViewport()->quad(pos, glm::vec2(scale), spriteComp.color, id, props);

        glm::vec3 textPos = glm::vec3(
            pos.x - (scale.x / 2.f) + labelLOffset,
            pos.y + (componentStyles.headerFontSize / 2.f) - 1.f, pos.z + 0.0005f);

        auto name = tagComp.name;
        m_sceneRef->getViewport()->msdfText(name, textPos, componentStyles.headerFontSize,
                                            ViewportTheme::colors.text, id);

        drawSlots(entity, simComp, transform);
    }

    glm::vec3 NodesArtist::getSlotPos(const Components::SlotComponent &comp, const Components::TransformComponent &parentTransform) {
        const auto pPos = parentTransform.position;
        const auto pScale = parentTransform.scale;

        auto slotdx = SLOT_DX;

        auto posX = pPos.x - pScale.x / 2.f;

        const bool isOutputSlot = comp.slotType == Components::SlotType::digitalOutput;
        if (isOutputSlot) {
            slotdx *= -1;
            posX += pScale.x;
        }

        posX += slotdx;
        float posY = pPos.y - pScale.y / 2.f + (SLOT_ROW_SIZE * comp.idx) + SLOT_ROW_SIZE / 2.f;

        const auto parentEntt = m_sceneRef->getEntityWithUuid(comp.parentId);
        if (!isHeaderLessComp(m_sceneRef->getEnttRegistry().get<Components::SimulationComponent>(parentEntt)))
            posY += SLOT_START_Y;

        return glm::vec3(posX, posY, pPos.z + 0.0005);
    }

    void NodesArtist::paintSlot(uint64_t id, uint64_t parentId, const glm::vec3 &pos,
                                float angle, const std::string &label, float labelDx,
                                SimEngine::LogicState state, bool isConnected, SimEngine::ExtendedPinType extendedType) const {
        auto bg = ViewportTheme::colors.stateLow;
        if (extendedType == SimEngine::ExtendedPinType::inputClock) {
            if ((bool)state) {
                bg = ViewportTheme::colors.clockConnectionHigh;
            } else {
                bg = ViewportTheme::colors.clockConnectionLow;
            }
        } else {
            switch (state) {
            case SimEngine::LogicState::low:
                bg = ViewportTheme::colors.stateLow;
                break;
            case SimEngine::LogicState::high:
                bg = ViewportTheme::colors.stateHigh;
                break;
            case SimEngine::LogicState::unknown:
                bg = ViewportTheme::colors.stateUnknow;
                break;
            case SimEngine::LogicState::high_z:
                bg = ViewportTheme::colors.stateHighZ;
                break;
            }
        }

        auto border = bg;
        if (!isConnected)
            bg.a = 0.1f;

        float ir = componentStyles.slotRadius - componentStyles.slotBorderSize;
        float r = componentStyles.slotRadius;

        if (extendedType == SimEngine::ExtendedPinType::inputClear) {
            QuadRenderProperties props;
            props.borderColor = border;
            props.borderRadius = glm::vec4(2.5f);
            props.borderSize = glm::vec4(componentStyles.slotBorderSize + 0.5);
            m_sceneRef->getViewport()->quad(pos, glm::vec2(r * 2.f), glm::vec4(0.f), id, props);
            props.borderSize = {};
            props.borderRadius = glm::vec4(1.5f);
            m_sceneRef->getViewport()->quad(pos, glm::vec2((ir - 1) * 2.f), glm::vec4(bg), id, props);
        } else {
            m_sceneRef->getViewport()->circle(pos, r, border, id, ir);
            m_sceneRef->getViewport()->circle(pos, ir - 1.f, bg, id);
        }

        float labelX = pos.x + labelDx;
        float dY = componentStyles.slotRadius - std::abs(componentStyles.slotRadius * 2.f - componentStyles.slotLabelSize) / 2.f;
        m_sceneRef->getViewport()->msdfText(label, {labelX, pos.y + dY, pos.z}, componentStyles.slotLabelSize, ViewportTheme::colors.text, parentId, angle);
    }

    void NodesArtist::drawSevenSegDisplay(
        entt::entity entity,
        const Components::TagComponent &tagComp,
        const Components::TransformComponent &transform,
        const Components::SpriteComponent &spriteComp,
        const Components::SimulationComponent &simComp) {

        const auto &pos = transform.position;
        const auto &rotation = transform.angle;
        const auto &scale = transform.scale;

        const float headerHeight = componentStyles.headerHeight;
        auto headerPos = glm::vec3(pos.x, pos.y - scale.y / 2.f + headerHeight / 2.f, pos.z);

        const auto &registry = m_sceneRef->getEnttRegistry();
        bool isSelected = registry.any_of<Components::SelectedComponent>(entity);
        auto border = isSelected ? ViewportTheme::colors.selectedComp : spriteComp.borderColor;

        uint64_t id = (uint64_t)entity;

        glm::vec3 textPos = glm::vec3(pos.x - scale.x / 2.f + componentStyles.paddingX, headerPos.y + componentStyles.paddingY, pos.z + 0.0005f);

        QuadRenderProperties props;
        props = {};
        props.angle = rotation;
        props.borderRadius = spriteComp.borderRadius;
        props.borderSize = spriteComp.borderSize;
        props.borderColor = border;
        props.isMica = true;

        m_sceneRef->getViewport()->quad(pos, glm::vec2(scale), spriteComp.color, id, props);

        props = {};
        props.angle = rotation;
        props.borderSize = glm::vec4(0.f);
        props.borderRadius = glm::vec4(0, 0, spriteComp.borderRadius.x - spriteComp.borderSize.x, spriteComp.borderRadius.y - spriteComp.borderSize.y);
        props.isMica = true;

        m_sceneRef->getViewport()->quad(headerPos,
                                        glm::vec2(scale.x - spriteComp.borderSize.w - spriteComp.borderSize.y, headerHeight - spriteComp.borderSize.x - spriteComp.borderSize.z),
                                        spriteComp.headerColor,
                                        id,
                                        props);

        m_sceneRef->getViewport()->msdfText(tagComp.name, textPos, componentStyles.headerFontSize, ViewportTheme::colors.text, id, rotation);

        {
            auto compState = SimEngine::SimulationEngine::instance().getComponentState(simComp.simEngineEntity);
            auto tex = m_artistTools.sevenSegDispTexs[0];
            auto texSize = tex->getScale();
            float texWidth = 60;
            float texHeight = (texSize.y / texSize.x) * texWidth;
            float posX = transform.position.x + (transform.scale.x / 2.f);
            posX -= texWidth / 2.f + componentStyles.paddingX;
            glm::vec3 texPos = {posX,
                                transform.position.y + (headerHeight / 2.f),
                                transform.position.z + 0.0001};
            m_sceneRef->getViewport()->quad(texPos, {texWidth, texHeight}, glm::vec4(1.f), (int)entity);

            for (int i = 0; i < (int)compState.inputStates.size(); i++) {
                if (!compState.inputStates[i])
                    continue;
                tex = m_artistTools.sevenSegDispTexs[i + 1];
                m_sceneRef->getViewport()->quad(texPos, {texWidth, texHeight}, glm::vec4(1.f), (int)entity);
            }
        }

        drawSlots(entity, simComp, transform);
    }

    void NodesArtist::drawSlots(const entt::entity parentEntt, const Components::SimulationComponent &comp, const Components::TransformComponent &transformComp) {
        const auto def = SimEngine::ComponentCatalog::instance().getComponentDefinition(comp.type);
        auto &registry = m_sceneRef->getEnttRegistry();
        const auto slotsView = registry.view<Components::SlotComponent>();

        const float labeldx = componentStyles.slotMargin + (componentStyles.slotRadius * 2.f);

        const auto compState = SimEngine::SimulationEngine::instance().getComponentState(comp.simEngineEntity);

        const float angle = transformComp.angle;
        auto [inpDetails, outDetails] = def->getPinDetails();

        std::string label;
        for (size_t i = 0; i < comp.inputSlots.size(); i++) {
            auto slot = m_sceneRef->getEntityWithUuid(comp.inputSlots[i]);
            const auto state = compState.inputStates[i];
            const auto isConnected = compState.inputConnected[i];
            auto &slotComp = slotsView.get<Components::SlotComponent>(slot);
            auto slotPos = getSlotPos(slotComp, transformComp);
            const uint64_t parentId = (uint64_t)m_sceneRef->getEntityWithUuid(slotComp.parentId);
            label = inpDetails.size() > i ? inpDetails[i].name : "X" + std::to_string(i);
            paintSlot((uint64_t)slot, parentId, slotPos, angle, label, labeldx, state.state, isConnected,
                      inpDetails.size() > i ? inpDetails[i].extendedType : SimEngine::ExtendedPinType::none);
        }

        for (size_t i = 0; i < comp.outputSlots.size(); i++) {
            auto slot = m_sceneRef->getEntityWithUuid(comp.outputSlots[i]);
            const auto state = compState.outputStates[i];
            const auto isConnected = compState.outputConnected[i];
            auto &slotComp = slotsView.get<Components::SlotComponent>(slot);
            auto slotPos = getSlotPos(slotComp, transformComp);
            const uint64_t parentId = (uint64_t)m_sceneRef->getEntityWithUuid(slotComp.parentId);
            label = outDetails.size() > i ? outDetails[i].name : "Y" + std::to_string(i);
            const float labelWidth = m_sceneRef->getViewport()->getMSDFTextRenderSize(label, componentStyles.slotLabelSize).x;
            paintSlot((uint64_t)slot, parentId, slotPos, angle, label, -labeldx - labelWidth, state.state, isConnected,
                      outDetails.size() > i ? outDetails[i].extendedType : SimEngine::ExtendedPinType::none);
        }
    }
} // namespace Bess::Canvas
