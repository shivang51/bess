#include "scene/artist/schematic_artist.h"
#include "component_catalog.h"
#include "component_types/component_types.h"
#include "entt/entity/fwd.hpp"
#include "ext/vector_float3.hpp"
#include "scene/components/components.h"
#include "scene/renderer/renderer.h"
#include "scene/scene.h"
#include "settings/viewport_theme.h"
#include <cstdint>
#include <string>

using Renderer = Bess::Renderer2D::Renderer;

namespace Bess::Canvas {

    constexpr struct SchematicViewCompStyles {
        float paddingX = 8.f;
        float paddingY = 4.f;

        float pinSize = 20.f;
        float pinRowGap = 12.f;
        float pinLabelSize = 10.f;

        float nameFontSize = 10.f;

        float strokeSize = 2.f;

        float negCircleR = 4.f, negCircleOff = negCircleR * 2.f;
    } schematicCompStyles;

    constexpr float SCHEMATIC_VIEW_PIN_ROW_SIZE = schematicCompStyles.nameFontSize + schematicCompStyles.strokeSize + schematicCompStyles.pinRowGap;

    SchematicArtist::SchematicArtist(Scene *scene) : BaseArtist(scene) {
    }

    void SchematicArtist::drawSimEntity(
        entt::entity entity,
        const Components::TagComponent &tagComp,
        const Components::TransformComponent &transform,
        const Components::SpriteComponent &spriteComp,
        const Components::SimulationComponent &simComponent) {

        auto &registry = m_sceneRef->getEnttRegistry();

        if (tagComp.isSimComponent && tagComp.type.simCompType == SimEngine::ComponentType::SEVEN_SEG_DISPLAY) {
            drawSevenSegDisplay(entity, tagComp, transform, spriteComp, simComponent);
            return;
        }

        paintSchematicView(entity, tagComp, transform, spriteComp, simComponent);
    }

    void SchematicArtist::paintSchematicView(entt::entity entity,
                                             const Components::TagComponent &tagComp,
                                             const Components::TransformComponent &transform,
                                             const Components::SpriteComponent &spriteComp,
                                             const Components::SimulationComponent &simComponent) {
        const auto &registry = m_sceneRef->getEnttRegistry();
        SimEngine::ComponentType type = simComponent.type;

        const auto &pos = transform.position;
        const auto &scale = transform.scale;

        auto schematicInfo = getCompSchematicInfo(entity);

        if (!schematicInfo.shouldDraw)
            return;

        const auto &textColor = ViewportTheme::schematicViewColors.text;
        const auto &fillColor = ViewportTheme::schematicViewColors.componentFill;
        const auto &strokeColor = ViewportTheme::schematicViewColors.componentStroke;

        float nodeWeight = schematicCompStyles.strokeSize;
        float negCircleR = schematicCompStyles.negCircleR;

        auto negateCircleAt = [&](glm::vec3 pos) {
            Renderer::circle(pos, negCircleR, strokeColor, -1, negCircleR - nodeWeight);
        };

        const int id = (uint64_t)entity;

        const float w = schematicInfo.width, h = schematicInfo.height;
        const float x = pos.x - w / 2, x1 = pos.x + w / 2;
        const float y = pos.y - h / 2, y1 = pos.y + h / 2;
        const float cpXL = x + (w * 0.25);
        const float rb = schematicInfo.outPinStart;

#if DEBUG & 0 // drawing bounding box
        {
            static const glm::vec4 col = glm::vec4(1.f, 0.f, 0.f, 1.f);
            Renderer::beginPathMode({x, y, 2.f}, nodeWeight, col, id);
            Renderer::pathLineTo({x1, y, 2.f}, nodeWeight, col, id);
            Renderer::pathLineTo({x1, y1, 2.f}, nodeWeight, col, id);
            Renderer::pathLineTo({x, y1, 2.f}, nodeWeight, col, id);
            Renderer::endPathMode(true);
        }
#endif

        bool showName = false;

        switch (type) {
        case SimEngine::ComponentType::AND:
        case SimEngine::ComponentType::NAND: {
            const float curveStartX = x1 - (w * 0.25);
            const float cpX = curveStartX + (w * 0.33);
            const auto cp1 = glm::vec2{cpX, y + (h * 0.25)};
            const auto cp2 = glm::vec2{cpX, y1 - (h * 0.25)};
            // diagram
            Renderer::beginPathMode({x, y, pos.z}, nodeWeight, strokeColor, id);
            Renderer::pathLineTo({curveStartX, y, pos.z}, nodeWeight, strokeColor, id);
            Renderer::pathCubicBeizerTo({curveStartX, y1, pos.z}, cp1, cp2, nodeWeight, strokeColor, id);
            Renderer::pathLineTo({x, y1, pos.z}, nodeWeight, strokeColor, id);
            Renderer::endPathMode(true, true, fillColor);
        } break;
        case SimEngine::ComponentType::OR:
        case SimEngine::ComponentType::NOR: {
            const float curveStartX = x1 - (w * 0.3f);
            const float cpX = curveStartX + (w * 0.20);
            // diagram
            Renderer::beginPathMode({x, y, pos.z}, nodeWeight, strokeColor, id);
            Renderer::pathQuadBeizerTo({x, y1, pos.z}, {cpXL, y + (y1 - y) / 2},
                                       nodeWeight, strokeColor, id);
            Renderer::pathLineTo({curveStartX, y1, pos.z}, nodeWeight, strokeColor, id);
            Renderer::pathQuadBeizerTo({x1, y + (h * 0.5f), pos.z},
                                       {cpX, y + (h * 0.85)},
                                       nodeWeight, strokeColor, id);
            Renderer::pathQuadBeizerTo({curveStartX, y, pos.z},
                                       {cpX, y + (h * 0.15)},
                                       nodeWeight, strokeColor, id);
            Renderer::endPathMode(true, true, fillColor);

        } break;
        case SimEngine::ComponentType::XNOR:
        case SimEngine::ComponentType::XOR: {
            const float curveStartX = x1 - (w * 0.3f);
            const float cpX = curveStartX + (w * 0.20);

            // diagram
            Renderer::beginPathMode({x, y, pos.z}, nodeWeight, strokeColor, id);
            Renderer::pathQuadBeizerTo({x, y1, pos.z}, {cpXL, y + (y1 - y) / 2}, nodeWeight, strokeColor, id);
            Renderer::endPathMode(false);
            float gapX = 8.f;
            Renderer::beginPathMode({x + gapX, y, pos.z}, nodeWeight, strokeColor, id);
            Renderer::pathQuadBeizerTo({x + gapX, y1, pos.z}, {cpXL + gapX, y + (y1 - y) / 2}, nodeWeight, strokeColor, id);
            Renderer::pathLineTo({curveStartX, y1, pos.z}, nodeWeight, strokeColor, id);
            Renderer::pathQuadBeizerTo({x1, y + (h * 0.5f), pos.z},
                                       {cpX, y + (h * 0.85)},
                                       nodeWeight, strokeColor, id);
            Renderer::pathQuadBeizerTo({curveStartX, y, pos.z},
                                       {cpX, y + (h * 0.15)},
                                       nodeWeight, strokeColor, id);
            Renderer::endPathMode(true, true, fillColor);
        } break;
        case SimEngine::ComponentType::NOT: {
            Renderer::beginPathMode({x, y, pos.z}, nodeWeight, strokeColor, id);
            Renderer::pathLineTo({x1, y + (y1 - y) / 2.f, pos.z}, nodeWeight, strokeColor, id);
            Renderer::pathLineTo({x, y1, pos.z}, nodeWeight, strokeColor, id);
            Renderer::endPathMode(true, true, fillColor);
        } break;
        default:
            // a square with name in center
            showName = true;
            Renderer::beginPathMode({x, y, pos.z}, nodeWeight, strokeColor, id);
            Renderer::pathLineTo({x1, y, pos.z}, nodeWeight, strokeColor, id);
            Renderer::pathLineTo({x1, y1, pos.z}, nodeWeight, strokeColor, id);
            Renderer::pathLineTo({x, y1, pos.z}, nodeWeight, ViewportTheme::colors.wire, id);
            Renderer::endPathMode(true, true, fillColor);
            break;
        }

        // circle for negating components
        if (type == SimEngine::ComponentType::NAND || type == SimEngine::ComponentType::NOR || type == SimEngine::ComponentType::NOT || type == SimEngine::ComponentType::XNOR) {
            negateCircleAt({rb - negCircleR, y + (y1 - y) / 2.f, pos.z});
        }

        if (showName) {
            auto textSize = Renderer2D::Renderer::getMSDFTextRenderSize(tagComp.name, 10.f); // headerFontSize
            glm::vec3 textPos = {pos.x, y + (y1 - y) / 2.f, pos.z + 0.0005f};
            textPos.x -= textSize.x / 2.f;
            textPos.y += 10.f / 2.f; // headerFontSize / 2
            Renderer::msdfText(tagComp.name, textPos, schematicCompStyles.nameFontSize, textColor, id, 0.f);
        }

        drawSlots(entity, simComponent, transform);
    }

    void SchematicArtist::drawSlots(const entt::entity parentEntt, const Components::SimulationComponent &simComp, const Components::TransformComponent &transformComp) {
        auto def = SimEngine::ComponentCatalog::instance().getComponentDefinition(simComp.type);
        auto [inpDetails, outDetails] = def->getPinDetails();

        auto schematicInfo = getCompSchematicInfo(parentEntt);

        float inPinStart = schematicInfo.inpPinStart;
        float h = schematicInfo.height;
        const auto &pos = transformComp.position;
        const float y = pos.y - h / 2, y1 = pos.y + h / 2;

        const auto &pinColor = ViewportTheme::schematicViewColors.pin;
        float nodeWeight = schematicCompStyles.strokeSize;

        std::string label = "";
        // inputs
        {
            size_t inpCount = simComp.inputSlots.size();
            float yIncr = h / (inpCount + 1);
            for (int i = 0; i < inpCount; i++) {
                float pinY = y + yIncr * (i + 1);
                int pinId = (uint64_t)m_sceneRef->getEntityWithUuid(simComp.inputSlots[i]);
                Renderer::beginPathMode({inPinStart, pinY, pos.z - 0.0005f}, nodeWeight, pinColor, pinId);
                Renderer::pathLineTo({schematicInfo.inpConnStart, pinY, pos.z - 0.0005f}, nodeWeight, pinColor, pinId);
                Renderer::endPathMode(false);
                label = inpDetails.size() > i ? inpDetails[i].name : "X" + std::to_string(i);
                Renderer::msdfText(label,
                                   {schematicInfo.inpConnStart, pinY - nodeWeight, pos.z - 0.0005f},
                                   10.f, ViewportTheme::colors.text, (int)parentEntt, 0.f); // headerFontSize
            }
        }

        // outputs
        {
            size_t outCount = simComp.outputSlots.size();
            float yIncr = h / (outCount + 1);
            for (int i = 0; i < outCount; i++) {
                float pinY = y + yIncr * (i + 1);
                int pinId = (uint64_t)m_sceneRef->getEntityWithUuid(simComp.outputSlots[i]);
                Renderer::beginPathMode({schematicInfo.outPinStart, pinY, pos.z - 0.0005f}, nodeWeight, pinColor, pinId);
                Renderer::pathLineTo({schematicInfo.outConnStart, pinY, pos.z - 0.0005f}, nodeWeight, pinColor, pinId);
                Renderer::endPathMode(false);
                label = outDetails.size() > i ? outDetails[i].name : "Y" + std::to_string(i);
                float size = Renderer2D::Renderer::getMSDFTextRenderSize(label, 10.f).x; // headerFontSize
                Renderer::msdfText(label,
                                   {schematicInfo.outConnStart - size, pinY - nodeWeight, pos.z - 0.0005f},
                                   10.f, ViewportTheme::colors.text, (int)parentEntt, 0.f); // headerFontSize
            }
        }
    }

    ArtistCompSchematicInfo SchematicArtist::getCompSchematicInfo(entt::entity ent) {
        const auto &reg = m_sceneRef->getEnttRegistry();
        const auto &tagComp = reg.get<Components::TagComponent>(ent);
        const auto &simComp = reg.get<Components::SimulationComponent>(ent);
        const auto &transform = reg.get<Components::TransformComponent>(ent);
        const auto &scale = transform.scale;
        const auto &pos = transform.position;

        float n = std::max(simComp.inputSlots.size(), simComp.outputSlots.size());
        float h = (SCHEMATIC_VIEW_PIN_ROW_SIZE * n) + (schematicCompStyles.paddingY * 2.f);

        float w = h * 1.2f;
        float x = pos.x - w / 2, x1 = pos.x + w / 2;
        float y = pos.y - h / 2, y1 = pos.y + h / 2;

        float negCircleR = schematicCompStyles.negCircleR;
        float negCircleOff = schematicCompStyles.negCircleOff;

        ArtistCompSchematicInfo info;

        info.outPinStart = x1;

        switch (simComp.type) {
        case SimEngine::ComponentType::AND: {
            info.inpPinStart = x;
            info.inpConnStart = info.inpPinStart - schematicCompStyles.pinSize;
            info.outConnStart = info.outPinStart + schematicCompStyles.pinSize;
        } break;
        case SimEngine::ComponentType::NAND: {
            info.inpPinStart = x;
            info.outPinStart += negCircleOff;
            info.inpConnStart = info.inpPinStart - schematicCompStyles.pinSize;
            info.outConnStart = info.outPinStart + schematicCompStyles.pinSize;
        } break;
        case SimEngine::ComponentType::XOR:
        case SimEngine::ComponentType::OR: {
            info.inpPinStart = x + (w * 0.25) / 2.f;
            info.inpConnStart = x - schematicCompStyles.pinSize;
            info.outConnStart = info.outPinStart + schematicCompStyles.pinSize;
        } break;
        case SimEngine::ComponentType::XNOR:
        case SimEngine::ComponentType::NOR: {
            info.inpPinStart = x + (w * 0.25) / 2.f;
            info.outPinStart += negCircleOff;
            info.inpConnStart = x - schematicCompStyles.pinSize;
            info.outConnStart = info.outPinStart + schematicCompStyles.pinSize;
        } break;
        case SimEngine::ComponentType::NOT: {
            info.inpPinStart = x;
            info.outPinStart += negCircleOff;
            info.inpConnStart = info.inpPinStart - schematicCompStyles.pinSize;
            info.outConnStart = info.outPinStart + schematicCompStyles.pinSize;
        } break;
        // case SimEngine::ComponentType::INPUT:
        // case SimEngine::ComponentType::OUTPUT: {
        //     info.shouldDraw = false;
        // } break;
        default:
            w = Renderer::getMSDFTextRenderSize(tagComp.name, schematicCompStyles.nameFontSize).x + 8.f * 2.f; // paddingX * 2
            x = pos.x - w / 2, x1 = pos.x + w / 2;
            y = pos.y - h / 2, y1 = pos.y + h / 2;

            info.inpPinStart = x;
            info.outPinStart = x1;
            info.inpConnStart = info.inpPinStart - schematicCompStyles.pinSize;
            info.outConnStart = info.outPinStart + schematicCompStyles.pinSize;
        }

        info.width = w;
        info.height = h;
        return info;
    }

    ArtistCompSchematicInfo SchematicArtist::getCompSchematicInfo(UUID uuid) {
        return getCompSchematicInfo(m_sceneRef->getEntityWithUuid(uuid));
    }

    glm::vec3 SchematicArtist::getSlotPos(const Components::SlotComponent &comp,
                                          const Components::TransformComponent &parentTransform) {
        auto &registry = m_sceneRef->getEnttRegistry();
        auto parentEntt = m_sceneRef->getEntityWithUuid(comp.parentId);
        const auto &simComp = registry.get<Components::SimulationComponent>(parentEntt);

        const auto info = getCompSchematicInfo(parentEntt);

        float x = 0, y = parentTransform.position.y - info.height / 2.f;

        bool isOutputSlot = comp.slotType == Components::SlotType::digitalOutput;

        float yIncr = 0;
        if (isOutputSlot) {
            x = info.outConnStart;
            yIncr = info.height / (simComp.outputSlots.size() + 1);
        } else {
            x = info.inpConnStart;
            yIncr = info.height / (simComp.inputSlots.size() + 1);
        }

        y += yIncr * (comp.idx + 1);

        return {x, y, parentTransform.position.z + 0.0005};
    }
} // namespace Bess::Canvas
