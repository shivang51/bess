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
        float slotMargin = 4.f;
        float slotBorderSize = 0.5f;
        float rowMargin = 4.f;
        float rowGap = 8.f;
        float slotLabelSize = 10.f;
    } componentStyles;

    float SLOT_DX = componentStyles.paddingX + componentStyles.slotRadius + componentStyles.slotMargin;
    float SLOT_START_Y = componentStyles.headerHeight + componentStyles.rowMargin + componentStyles.slotRadius;
    float SLOT_ROW_SIZE = componentStyles.rowMargin + (componentStyles.slotRadius * 2.f) + componentStyles.rowGap;

    glm::vec3 Artist::getSlotPos(const Components::SlotComponent &comp) {
        auto &registry = sceneRef->getEnttRegistry();
        auto &pTransform = registry.get<Components::TransformComponent>((entt::entity)comp.parentId);
        auto [pPos, _, pScale] = pTransform.decompose();

        auto slotdx = SLOT_DX;

        auto posX = pPos.x - pScale.x / 2.f;

        bool isOutputSlot = comp.slotType == Components::SlotType::digitalOutput;
        if (isOutputSlot) {
            slotdx *= -1;
            posX += pScale.x;
        }

        posX += slotdx;
        float posY = pPos.y - pScale.y / 2.f + SLOT_START_Y + (SLOT_ROW_SIZE * comp.idx);

        return glm::vec3(posX, posY, pPos.z + 0.002f);
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

            auto bgColor = isHigh ? ViewportTheme::stateHighColor : ViewportTheme::componentBGColor;

            Renderer::quad(slotPos, glm::vec2(componentStyles.slotRadius * 2.f), bgColor, (uint64_t)slot, 0.f,
                           glm::vec4(componentStyles.slotRadius),
                           glm::vec4(componentStyles.slotBorderSize), ViewportTheme::stateLowColor, false);

            float labelX = slotPos.x + labeldx;
            Renderer::text("X" + std::to_string(slotComp.idx), {labelX, slotPos.y + (componentStyles.slotLabelSize / 2.f) - 1.f, slotPos.z}, componentStyles.slotLabelSize, ViewportTheme::textColor * 0.9f, slotComp.parentId);
        }

        float labelWidth = (Renderer::getCharRenderSize('Z', componentStyles.slotLabelSize).x * 2.f);
        for (size_t i = 0; i < comp.outputSlots.size(); i++) {
            auto slot = comp.outputSlots[i];
            auto isHigh = compState.outputStates[i];
            auto &slotComp = registry.get<Components::SlotComponent>(slot);
            auto slotPos = getSlotPos(slotComp);

            auto bgColor = isHigh ? ViewportTheme::stateHighColor : ViewportTheme::componentBGColor;
            Renderer::quad(slotPos, glm::vec2(componentStyles.slotRadius * 2.f), bgColor, (uint64_t)slot, 0.f,
                           glm::vec4(componentStyles.slotRadius),
                           glm::vec4(componentStyles.slotBorderSize), ViewportTheme::stateLowColor, false);
            float labelX = slotPos.x - labeldx - labelWidth;

            Renderer::text("Y" + std::to_string(slotComp.idx), {labelX, slotPos.y + (componentStyles.slotLabelSize / 2.f) - 1.f, slotPos.z}, componentStyles.slotLabelSize, ViewportTheme::textColor * 0.9f, slotComp.parentId);

            slotPos.y += componentStyles.rowMargin + (componentStyles.slotRadius * 2.f) + componentStyles.rowGap;
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

    void Artist::drawConnection(entt::entity startEntity, entt::entity endEntity) {
        auto &registry = sceneRef->getEnttRegistry();
        auto startPos = Artist::getSlotPos(registry.get<Components::SlotComponent>(startEntity));
        auto endPos = Artist::getSlotPos(registry.get<Components::SlotComponent>(endEntity));
        startPos.z = 0.f;
        endPos.z = 0.f;

        auto midX = startPos.x + ((endPos.x - startPos.x) / 2.f);

        std::vector<glm::vec3> points;
        points.emplace_back(startPos);
        points.emplace_back(glm::vec3(midX, startPos.y, 0.f));
        points.emplace_back(glm::vec3(midX, endPos.y, 0.f));
        points.emplace_back(glm::vec3(endPos));

        Renderer::drawPath(points, 2.f, ViewportTheme::wireColor, -1);
    }

    void Artist::drawConnectionEntity(entt::entity entity) {
        auto &registry = sceneRef->getEnttRegistry();

        auto &connComp = registry.get<Components::ConnectionComponent>(entity);
        Artist::drawConnection(connComp.slotAEntity, connComp.slotBEntity);
    }

    void Artist::drawSimEntity(entt::entity entity) {
        auto &registry = sceneRef->getEnttRegistry();

        auto &transformComp = registry.get<Components::TransformComponent>(entity);
        auto &spriteComp = registry.get<Components::SpriteComponent>(entity);
        auto &tagComp = registry.get<Components::TagComponent>(entity);
        auto &simComp = registry.get<Components::SimulationComponent>(entity);

        auto [pos, rotation, scale] = transformComp.decompose();
        float rowSize = componentStyles.rowMargin + (componentStyles.slotRadius * 2.f) + componentStyles.rowGap;
        int maxRows = std::max(simComp.inputSlots.size(), simComp.outputSlots.size());
        scale.y = componentStyles.headerHeight + componentStyles.rowMargin + componentStyles.slotRadius + (maxRows * rowSize);
        transformComp.scale(scale);

        float headerHeight = componentStyles.headerHeight;
        float radius = headerHeight / 2.f;
        auto headerPos = glm::vec3(pos.x, pos.y - scale.y / 2.f + headerHeight / 2.f, pos.z);

        spriteComp.borderRadius = glm::vec4(radius);

        uint64_t id = (uint64_t)entity;

        glm::vec3 textPos = glm::vec3(pos.x - scale.x / 2.f + componentStyles.paddingX, headerPos.y + componentStyles.paddingY, pos.z + 0.001f);

        Renderer::quad(pos, glm::vec2(scale), spriteComp.color, id, 0.f,
                       spriteComp.borderRadius,
                       spriteComp.borderSize, spriteComp.borderColor, true);

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
