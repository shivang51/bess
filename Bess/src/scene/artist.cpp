#include "scene/artist.h"
#define GLM_FORCE_DEGREES
#include "entt/entity/fwd.hpp"
#include "ext/vector_float3.hpp"
#include "geometric.hpp"
#include "imgui.h"
#include "scene/components/components.h"
#include "scene/renderer/renderer.h"
#include "settings/viewport_theme.h"
#include "simulation_engine.h"
#include <cstdint>
#include <semaphore.h>
#include <string>
#include <vector>

using Renderer = Bess::Renderer2D::Renderer;

namespace Bess::Canvas {
    Scene *Artist::sceneRef = nullptr;

    struct ComponentStyles {
        float headerHeight = 18.f;
        float headerFontSize = 10.f;
        float paddingX = 8.f;
        float paddingY = 4.f;
        float slotRadius = 4.5f;
        float slotMargin = 2.f;
        float slotBorderSize = 1.f;
        float rowMargin = 4.f;
        float rowGap = 4.f;
        float slotLabelSize = 10.f;
    } componentStyles;

    float SLOT_DX = componentStyles.paddingX + componentStyles.slotRadius + componentStyles.slotMargin;
    float SLOT_START_Y = componentStyles.headerHeight;
    float SLOT_ROW_SIZE = (componentStyles.rowMargin * 2.f) + (componentStyles.slotRadius * 2.f) + componentStyles.rowGap;

    glm::vec3 Artist::getSlotPos(const Components::SlotComponent &comp) {
        auto &registry = sceneRef->getEnttRegistry();
        auto parentEntt = sceneRef->getEntityWithUuid(comp.parentId);
        auto &pTransform = registry.get<Components::TransformComponent>(parentEntt);

        auto pPos = pTransform.position;
        auto pScale = pTransform.scale;

        bool isNonHeader = registry.any_of<Components::SimulationInputComponent, Components::SimulationOutputComponent>(parentEntt);

        auto slotdx = SLOT_DX;

        auto posX = pPos.x - pScale.x / 2.f;

        bool isOutputSlot = comp.slotType == Components::SlotType::digitalOutput;
        if (isOutputSlot) {
            slotdx *= -1;
            posX += pScale.x;
        }

        posX += slotdx;
        float posY = pPos.y - pScale.y / 2.f + (SLOT_ROW_SIZE * comp.idx) + SLOT_ROW_SIZE / 2.f;

        if (!isNonHeader)
            posY += SLOT_START_Y;

        return glm::vec3(posX, posY, pPos.z + 0.0005);
    }

    void Artist::paintSlot(uint64_t id, int idx, uint64_t parentId, const glm::vec3 &pos, float angle, const std::string &label, float labelDx, bool isHigh) {
        auto bgColor = isHigh ? ViewportTheme::stateHighColor : ViewportTheme::componentBGColor;
        auto borderColor = isHigh ? ViewportTheme::stateHighColor : ViewportTheme::stateLowColor;
        Renderer::quad(pos, glm::vec2(componentStyles.slotRadius * 2.f), ViewportTheme::componentBGColor, id, angle,
                       glm::vec4(componentStyles.slotRadius),
                       glm::vec4(componentStyles.slotBorderSize), borderColor, false);

        float r = componentStyles.slotRadius - componentStyles.slotBorderSize - 1.f;
        Renderer::quad(pos, glm::vec2(r * 2.f), bgColor, id, angle, glm::vec4(r),
                       glm::vec4(componentStyles.slotBorderSize), ViewportTheme::componentBGColor, false);

        float labelX = pos.x + labelDx;
        float dY = componentStyles.slotRadius - std::abs(componentStyles.slotRadius * 2.f - componentStyles.slotLabelSize) / 2.f;
        Renderer::text(label + std::to_string(idx), {labelX, pos.y + dY, pos.z}, componentStyles.slotLabelSize, ViewportTheme::textColor * 0.9f, parentId, angle);
    }

    void Artist::drawSlots(const Components::SimulationComponent &comp, const glm::vec3 &componentPos, float width, float angle) {
        auto &registry = sceneRef->getEnttRegistry();

        float labeldx = componentStyles.slotMargin + (componentStyles.slotRadius * 2.f);

        auto compState = SimEngine::SimulationEngine::instance().getComponentState(comp.simEngineEntity);

        for (size_t i = 0; i < comp.inputSlots.size(); i++) {
            auto slot = sceneRef->getEntityWithUuid(comp.inputSlots[i]);
            auto isHigh = compState.inputStates[i];
            auto &slotComp = registry.get<Components::SlotComponent>(slot);
            auto slotPos = getSlotPos(slotComp);
            uint64_t parentId = (uint64_t)sceneRef->getEntityWithUuid(slotComp.parentId);
            paintSlot((uint64_t)slot, slotComp.idx, parentId, slotPos, angle, "X", labeldx, isHigh);
        }

        float labelWidth = (Renderer::getCharRenderSize('Z', componentStyles.slotLabelSize).x * 2.f);
        labeldx += labelWidth;
        for (size_t i = 0; i < comp.outputSlots.size(); i++) {
            auto slot = sceneRef->getEntityWithUuid(comp.outputSlots[i]);
            auto isHigh = compState.outputStates[i];
            auto &slotComp = registry.get<Components::SlotComponent>(slot);
            auto slotPos = getSlotPos(slotComp);
            uint64_t parentId = (uint64_t)sceneRef->getEntityWithUuid(slotComp.parentId);
            paintSlot((uint64_t)slot, slotComp.idx, parentId, slotPos, angle, "Y", -labeldx, isHigh);
        }
    }

    void Artist::drawGhostConnection(const entt::entity &startEntity, const glm::vec2 pos) {
        auto &registry = sceneRef->getEnttRegistry();
        auto startPos = Artist::getSlotPos(registry.get<Components::SlotComponent>(startEntity));
        startPos.z = 0.f;

        auto midX = startPos.x + ((pos.x - startPos.x) / 2.f);

        std::vector<glm::vec3> points;
        points.emplace_back(startPos);
        points.emplace_back(glm::vec3(midX, startPos.y, 0.f));
        points.emplace_back(glm::vec3(midX, pos.y, 0.f));
        points.emplace_back(glm::vec3(pos, 0.f));

        Renderer::drawPath(points, 2.f, ViewportTheme::wireColor, -1);
    }

    void Artist::drawConnection(const UUID &id, entt::entity inputEntity, entt::entity outputEntity, bool isSelected) {
        auto &registry = sceneRef->getEnttRegistry();
        auto connEntity = sceneRef->getEntityWithUuid(id);
        auto &connectionComponent = registry.get<Components::ConnectionComponent>(connEntity);
        auto &outputSlotComp = registry.get<Components::SlotComponent>(outputEntity);
        auto &simComp = registry.get<Components::SimulationComponent>(sceneRef->getEntityWithUuid(outputSlotComp.parentId));

        auto startPos = Artist::getSlotPos(registry.get<Components::SlotComponent>(inputEntity));
        auto endPos = Artist::getSlotPos(outputSlotComp);

        startPos.z = 0.f;
        endPos.z = 0.f;
        glm::vec4 color;
        if (connectionComponent.useCustomColor) {
            auto &spriteComponent = registry.get<Components::SpriteComponent>(connEntity);
            color = spriteComponent.color;
        } else {
            bool isHigh = SimEngine::SimulationEngine::instance().getComponentState(simComp.simEngineEntity).outputStates[outputSlotComp.idx];
            color = isHigh ? ViewportTheme::stateHighColor : ViewportTheme::stateLowColor;
        }
        color = isSelected ? ViewportTheme::selectedWireColor : color;
        auto connSegEntt = sceneRef->getEntityWithUuid(connectionComponent.segmentHead);
        auto connSegComp = registry.get<Components::ConnectionSegmentComponent>(connSegEntt);
        auto segId = connectionComponent.segmentHead;
        auto prevPos = startPos;

        while (connSegComp.next != UUID::null) {
            auto newSegId = connSegComp.next;
            auto newSegEntt = sceneRef->getEntityWithUuid(connSegComp.next);
            connSegComp = registry.get<Components::ConnectionSegmentComponent>(newSegEntt);

            glm::vec3 pos = glm::vec3(connSegComp.pos, prevPos.z);
            if (pos.x == 0.f) {
                pos.x = prevPos.x;
                if (connSegComp.isTail())
                    pos.y = endPos.y;
            } else {
                pos.y = prevPos.y;
                if (connSegComp.isTail())
                    pos.x = endPos.x;
            }

            auto segEntt = sceneRef->getEntityWithUuid(segId);
            bool isHovered = registry.all_of<Components::HoveredEntityComponent>(segEntt);
            auto size = isHovered ? 3.0 : 2.f;
            auto offPos = pos;
            auto offSet = (prevPos.y <= pos.y) ? size / 2.f : -size / 2.f;
            if (std::abs(prevPos.x - pos.x) <= 0.0001f) { // veritcal
                offPos.y += offSet;
                prevPos.y -= offSet;
            }
            Renderer::line(prevPos, offPos, size, color, (uint64_t)segEntt);
            segId = newSegId;
            prevPos = pos;
        }

        auto segEntt = sceneRef->getEntityWithUuid(segId);
        bool isHovered = registry.all_of<Components::HoveredEntityComponent>(segEntt);
        auto size = isHovered ? 3.0 : 2.f;
        auto offSet = (prevPos.y <= endPos.y) ? size / 2.f : -size / 2.f;
        if (std::abs(prevPos.x - endPos.x) <= 0.0001f) { // veritcal
            endPos.y += offSet;
            prevPos.y -= offSet;
        }
        Renderer::line(prevPos, endPos, size, color, (uint64_t)segEntt);
    }

    void Artist::drawConnectionEntity(entt::entity entity) {
        auto &registry = sceneRef->getEnttRegistry();

        auto &connComp = registry.get<Components::ConnectionComponent>(entity);
        auto &idComp = registry.get<Components::IdComponent>(entity);
        bool isSelected = registry.all_of<Components::SelectedComponent>(entity);
        Artist::drawConnection(idComp.uuid, sceneRef->getEntityWithUuid(connComp.inputSlot), sceneRef->getEntityWithUuid(connComp.outputSlot), isSelected);
    }

    void Artist::drawOutput(entt::entity entity) {
        auto &registry = sceneRef->getEnttRegistry();

        auto &transformComp = registry.get<Components::TransformComponent>(entity);
        auto &spriteComp = registry.get<Components::SpriteComponent>(entity);
        auto &tagComp = registry.get<Components::TagComponent>(entity);
        auto &simComp = registry.get<Components::SimulationComponent>(entity);
        auto &idComp = registry.get<Components::IdComponent>(entity);

        uint64_t id = (uint64_t)entity;
        auto pos = transformComp.position;
        auto rotation = transformComp.angle;
        auto scale = transformComp.scale;

        scale.y = SLOT_ROW_SIZE;
        scale.x = 90.f;
        transformComp.scale = scale;

        bool isSelected = registry.any_of<Components::SelectedComponent>(entity);
        auto borderColor = isSelected ? ViewportTheme::selectedCompColor : spriteComp.borderColor;

        Renderer::quad(pos, glm::vec2(scale), spriteComp.color, id, 0.f,
                       spriteComp.borderRadius,
                       spriteComp.borderSize, borderColor, true);

        float yOff = componentStyles.headerFontSize / 2.f - 2.f;
        auto labelSize = Renderer::getCharRenderSize('Z', componentStyles.headerFontSize).x * tagComp.name.size();
        glm::vec3 textPos = glm::vec3(pos.x + scale.x / 2.f - labelSize, pos.y + yOff, pos.z + 0.0005f);
        Renderer::text(tagComp.name, textPos, componentStyles.headerFontSize, ViewportTheme::textColor, id);
        drawSlots(simComp, pos, scale.x, rotation);
    }

    void Artist::drawInput(entt::entity entity) {
        auto &registry = sceneRef->getEnttRegistry();

        auto &transformComp = registry.get<Components::TransformComponent>(entity);
        auto &spriteComp = registry.get<Components::SpriteComponent>(entity);
        auto &tagComp = registry.get<Components::TagComponent>(entity);
        auto &simComp = registry.get<Components::SimulationComponent>(entity);
        auto &simInComp = registry.get<Components::SimulationInputComponent>(entity);
        auto &idComp = registry.get<Components::IdComponent>(entity);

        uint64_t id = (uint64_t)entity;
        auto pos = transformComp.position;
        auto rotation = transformComp.angle;
        auto scale = transformComp.scale;
        scale.y = SLOT_ROW_SIZE;
        scale.x = 80.f;
        transformComp.scale = scale;

        bool isSelected = registry.any_of<Components::SelectedComponent>(entity);
        auto borderColor = isSelected ? ViewportTheme::selectedCompColor : spriteComp.borderColor;

        Renderer::quad(pos, glm::vec2(scale), spriteComp.color, id, 0.f,
                       spriteComp.borderRadius,
                       spriteComp.borderSize, borderColor, true);

        float yOff = componentStyles.headerFontSize / 2.f - 2.f;
        glm::vec3 textPos = glm::vec3(pos.x - scale.x / 2.f + componentStyles.paddingX, pos.y + yOff, pos.z + 0.0005f);

        auto name = tagComp.name;
        if (simInComp.clockBhaviour) {
            /*name = "¤ " + name;*/
        }
        Renderer::text(name, textPos, componentStyles.headerFontSize, ViewportTheme::textColor, id);
        drawSlots(simComp, pos, scale.x, rotation);
    }

    void Artist::drawSimEntity(entt::entity entity) {
        auto &registry = sceneRef->getEnttRegistry();
        if (registry.all_of<Components::SimulationInputComponent>(entity)) {
            drawInput(entity);
            return;
        } else if (registry.all_of<Components::SimulationOutputComponent>(entity)) {
            drawOutput(entity);
            return;
        }

        auto &transformComp = registry.get<Components::TransformComponent>(entity);
        auto &spriteComp = registry.get<Components::SpriteComponent>(entity);
        auto &tagComp = registry.get<Components::TagComponent>(entity);
        auto &simComp = registry.get<Components::SimulationComponent>(entity);

        auto pos = transformComp.position;
        auto rotation = transformComp.angle;
        auto scale = transformComp.scale;
        int maxRows = std::max(simComp.inputSlots.size(), simComp.outputSlots.size());
        scale.y = componentStyles.headerHeight + componentStyles.rowGap + (maxRows * SLOT_ROW_SIZE);
        transformComp.scale = scale;

        float headerHeight = componentStyles.headerHeight;
        auto headerPos = glm::vec3(pos.x, pos.y - scale.y / 2.f + headerHeight / 2.f, pos.z);

        /*spriteComp.borderRadius = glm::vec4(radius);*/
        bool isSelected = registry.any_of<Components::SelectedComponent>(entity);
        auto borderColor = isSelected ? ViewportTheme::selectedCompColor : spriteComp.borderColor;

        uint64_t id = (uint64_t)entity;

        glm::vec3 textPos = glm::vec3(pos.x - scale.x / 2.f + componentStyles.paddingX, headerPos.y + componentStyles.paddingY, pos.z + 0.0005f);

        Renderer::quad(pos, glm::vec2(scale), spriteComp.color, id, rotation,
                       spriteComp.borderRadius,
                       spriteComp.borderSize, borderColor, true);

        Renderer::quad(headerPos,
                       glm::vec2(scale.x - spriteComp.borderSize.w - spriteComp.borderSize.y, headerHeight - spriteComp.borderSize.x - spriteComp.borderSize.z),
                       ViewportTheme::compHeaderColor,
                       id,
                       0.f,
                       glm::vec4(0, 0, spriteComp.borderRadius.x, spriteComp.borderRadius.y),
                       glm::vec4(0.f), glm::vec4(0.f), true);

        Renderer::text(tagComp.name, textPos, componentStyles.headerFontSize, ViewportTheme::textColor, id, rotation);

        drawSlots(simComp, glm::vec3(glm::vec2(pos) - (glm::vec2(scale) / 2.f), pos.z), scale.x, rotation);
    }
} // namespace Bess::Canvas
