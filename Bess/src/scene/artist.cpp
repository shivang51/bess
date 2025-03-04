#include "scene/artist.h"
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
        float headerHeight = 22.f;
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
        auto parentEntt = (entt::entity)comp.parentId;
        auto &pTransform = registry.get<Components::TransformComponent>(parentEntt);
        auto [pPos, _, pScale] = pTransform.decompose();

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

    void Artist::paintSlot(uint64_t id, int idx, uint64_t parentId, const glm::vec3 &pos, const std::string &label, float labelDx, bool isHigh) {
        auto bgColor = isHigh ? ViewportTheme::stateHighColor : ViewportTheme::componentBGColor;
        auto borderColor = isHigh ? ViewportTheme::stateHighColor : ViewportTheme::stateLowColor;
        Renderer::quad(pos, glm::vec2(componentStyles.slotRadius * 2.f), ViewportTheme::componentBGColor, id, 0.f,
                       glm::vec4(componentStyles.slotRadius),
                       glm::vec4(componentStyles.slotBorderSize), borderColor, false);

        float r = componentStyles.slotRadius - componentStyles.slotBorderSize - 1.f;
        Renderer::quad(pos, glm::vec2(r * 2.f), bgColor, id, 0.f, glm::vec4(r),
                       glm::vec4(componentStyles.slotBorderSize), ViewportTheme::componentBGColor, false);

        float labelX = pos.x + labelDx;
        float dY = componentStyles.slotRadius - std::abs(componentStyles.slotRadius * 2.f - componentStyles.slotLabelSize) / 2.f;
        Renderer::text(label + std::to_string(idx), {labelX, pos.y + dY, pos.z}, componentStyles.slotLabelSize, ViewportTheme::textColor * 0.9f, parentId);
    }

    void Artist::drawSlots(const Components::SimulationComponent &comp, const glm::vec3 &componentPos, float width) {
        auto &registry = sceneRef->getEnttRegistry();

        float labeldx = componentStyles.slotMargin + (componentStyles.slotRadius * 2.f);

        auto compState = SimEngine::SimulationEngine::instance().getComponentState(comp.simEngineEntity);

        for (size_t i = 0; i < comp.inputSlots.size(); i++) {
            auto slot = comp.inputSlots[i];
            auto isHigh = compState.inputStates[i];
            auto &slotComp = registry.get<Components::SlotComponent>(slot);
            auto slotPos = getSlotPos(slotComp);
            paintSlot((uint64_t)slot, slotComp.idx, slotComp.parentId, slotPos, "X", labeldx, isHigh);
        }

        float labelWidth = (Renderer::getCharRenderSize('Z', componentStyles.slotLabelSize).x * 2.f);
        labeldx += labelWidth;
        for (size_t i = 0; i < comp.outputSlots.size(); i++) {
            auto slot = comp.outputSlots[i];
            auto isHigh = compState.outputStates[i];
            auto &slotComp = registry.get<Components::SlotComponent>(slot);
            auto slotPos = getSlotPos(slotComp);
            paintSlot((uint64_t)slot, slotComp.idx, slotComp.parentId, slotPos, "Y", -labeldx, isHigh);
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

    void Artist::drawConnection(uint64_t id, entt::entity inputEntity, entt::entity outputEntity, bool isSelected) {
        auto &registry = sceneRef->getEnttRegistry();
        auto &outputSlotComp = registry.get<Components::SlotComponent>(outputEntity);
        auto startPos = Artist::getSlotPos(registry.get<Components::SlotComponent>(inputEntity));
        auto endPos = Artist::getSlotPos(outputSlotComp);
        startPos.z = 0.f;
        endPos.z = 0.f;

        auto midX = startPos.x + ((endPos.x - startPos.x) / 2.f);

        std::vector<glm::vec3> points;
        points.emplace_back(startPos);
        points.emplace_back(glm::vec3(midX, startPos.y, 0.f));
        points.emplace_back(glm::vec3(midX, endPos.y, 0.f));
        points.emplace_back(glm::vec3(endPos));

        auto &simComp = registry.get<Components::SimulationComponent>((entt::entity)outputSlotComp.parentId);
        bool isHigh = SimEngine::SimulationEngine::instance().getComponentState(simComp.simEngineEntity).outputStates[outputSlotComp.idx];

        auto color = isHigh ? ViewportTheme::stateHighColor : ViewportTheme::stateLowColor;
        color = isSelected ? ViewportTheme::selectedWireColor : color;

        Renderer::drawPath(points, 2.f, color, id);
    }

    void Artist::drawConnectionEntity(entt::entity entity) {
        auto &registry = sceneRef->getEnttRegistry();

        auto &connComp = registry.get<Components::ConnectionComponent>(entity);
        bool isSelected = registry.all_of<Components::SelectedComponent>(entity);
        Artist::drawConnection((uint64_t)entity, connComp.inputSlot, connComp.outputSlot, isSelected);
    }

    void Artist::drawOutput(entt::entity entity) {
        auto &registry = sceneRef->getEnttRegistry();

        auto &transformComp = registry.get<Components::TransformComponent>(entity);
        auto &spriteComp = registry.get<Components::SpriteComponent>(entity);
        auto &tagComp = registry.get<Components::TagComponent>(entity);
        auto &simComp = registry.get<Components::SimulationComponent>(entity);

        uint64_t id = (uint64_t)entity;
        auto [pos, rotation, scale] = transformComp.decompose();
        scale.y = SLOT_ROW_SIZE;
        scale.x = 90.f;
        transformComp.scale(scale);

        float radius = 4.f;
        spriteComp.borderRadius = glm::vec4(radius);

        Renderer::quad(pos, glm::vec2(scale), spriteComp.color, id, 0.f,
                       spriteComp.borderRadius,
                       spriteComp.borderSize, spriteComp.borderColor, true);

        float yOff = componentStyles.headerFontSize / 2.f - 2.f;
        auto labelSize = Renderer::getCharRenderSize('Z', componentStyles.headerFontSize).x * tagComp.name.size();
        glm::vec3 textPos = glm::vec3(pos.x + scale.x / 2.f - labelSize, pos.y + yOff, pos.z + 0.0005f);
        Renderer::text(tagComp.name, textPos, componentStyles.headerFontSize, ViewportTheme::textColor, id);
        drawSlots(simComp, pos, scale.x);
    }

    void Artist::drawInput(entt::entity entity) {
        auto &registry = sceneRef->getEnttRegistry();

        auto &transformComp = registry.get<Components::TransformComponent>(entity);
        auto &spriteComp = registry.get<Components::SpriteComponent>(entity);
        auto &tagComp = registry.get<Components::TagComponent>(entity);
        auto &simComp = registry.get<Components::SimulationComponent>(entity);

        uint64_t id = (uint64_t)entity;
        auto [pos, rotation, scale] = transformComp.decompose();
        scale.y = SLOT_ROW_SIZE;
        scale.x = 80.f;
        transformComp.scale(scale);

        float radius = 4.f;
        spriteComp.borderRadius = glm::vec4(radius);

        Renderer::quad(pos, glm::vec2(scale), spriteComp.color, id, 0.f,
                       spriteComp.borderRadius,
                       spriteComp.borderSize, spriteComp.borderColor, true);

        float yOff = componentStyles.headerFontSize / 2.f - 2.f;
        glm::vec3 textPos = glm::vec3(pos.x - scale.x / 2.f + componentStyles.paddingX, pos.y + yOff, pos.z + 0.0005f);
        Renderer::text(tagComp.name, textPos, componentStyles.headerFontSize, ViewportTheme::textColor, id);
        drawSlots(simComp, pos, scale.x);
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

        auto [pos, rotation, scale] = transformComp.decompose();
        int maxRows = std::max(simComp.inputSlots.size(), simComp.outputSlots.size());
        scale.y = componentStyles.headerHeight + componentStyles.rowGap + (maxRows * SLOT_ROW_SIZE);
        transformComp.scale(scale);

        float headerHeight = componentStyles.headerHeight;
        float radius = 4.f;
        auto headerPos = glm::vec3(pos.x, pos.y - scale.y / 2.f + headerHeight / 2.f, pos.z);

        spriteComp.borderRadius = glm::vec4(radius);
        bool isSelected = registry.any_of<Components::SelectedComponent>(entity);
        auto borderColor = isSelected ? ViewportTheme::selectedCompColor : spriteComp.color;

        uint64_t id = (uint64_t)entity;

        glm::vec3 textPos = glm::vec3(pos.x - scale.x / 2.f + componentStyles.paddingX, headerPos.y + componentStyles.paddingY, pos.z + 0.0005f);

        Renderer::quad(pos, glm::vec2(scale), spriteComp.color, id, 0.f,
                       spriteComp.borderRadius,
                       spriteComp.borderSize, borderColor, true);

        Renderer::quad(headerPos,
                       glm::vec2(scale.x, headerHeight), ViewportTheme::compHeaderColor,
                       id,
                       0.f,
                       glm::vec4(0.f, 0.f, radius, radius),
                       spriteComp.borderSize, glm::vec4(0.f), true);

        Renderer::text(tagComp.name, textPos, componentStyles.headerFontSize, ViewportTheme::textColor, id);

        drawSlots(simComp, glm::vec3(glm::vec2(pos) - (glm::vec2(scale) / 2.f), pos.z), scale.x);
    }
} // namespace Bess::Canvas
