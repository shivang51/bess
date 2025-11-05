#include "scene/artist/schematic_artist.h"
#include "common/log.h"
#include "component_catalog.h"
#include "entt/entity/fwd.hpp"
#include "ext/vector_float3.hpp"
#include "scene/renderer/vulkan/path_renderer.h"
#include "settings/viewport_theme.h"
#include "simulation_engine.h"
#include "vulkan_texture.h"
#include <memory>
#include <string>

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

    SchematicArtist::SchematicArtist(const std::shared_ptr<Vulkan::VulkanDevice> &device,
                                     const std::shared_ptr<Vulkan::VulkanOffscreenRenderPass> &renderPass,
                                     VkExtent2D extent) : BaseArtist(device, renderPass, extent) {
    }

    void SchematicArtist::drawSimEntity(
        const entt::entity entity,
        const Components::TagComponent &tagComp,
        const Components::TransformComponent &transform,
        const Components::SpriteComponent &spriteComp,
        const Components::SimulationComponent &simComponent) {

        const auto &catalog = SimEngine::ComponentCatalog::instance();

        if (catalog.isSpecialCompDef(simComponent.defHash, SimEngine::ComponentCatalog::SpecialType::sevenSegmentDisplay)) {
            drawSevenSegDisplay(entity, tagComp, transform, spriteComp, simComponent);
        } else {
            paintSchematicView(entity, tagComp, transform, spriteComp, simComponent);
        }

        drawSlots(entity, simComponent, transform);
    }

    void SchematicArtist::drawSevenSegDisplay(entt::entity entity,
                                              const Components::TagComponent &tagComp,
                                              const Components::TransformComponent &transform,
                                              const Components::SpriteComponent &spriteComp,
                                              const Components::SimulationComponent &simComp) const {
        const auto schematicInfo = getCompSchematicInfo(entity);
        const auto &textColor = ViewportTheme::schematicViewColors.text;
        const auto &fillColor = ViewportTheme::schematicViewColors.componentFill;
        const auto &strokeColor = ViewportTheme::schematicViewColors.componentStroke;
        constexpr float strokeSize = schematicCompStyles.strokeSize;

        const int id = static_cast<int>(entity);
        const auto &pos = transform.position;

        const float w = schematicInfo.width, h = schematicInfo.height;
        const float x = pos.x - (w / 2), x1 = pos.x + (w / 2);
        const float y = pos.y - (h / 2), y1 = pos.y + (h / 2);

        m_pathRenderer->beginPathMode({x, y, pos.z}, strokeSize, strokeColor, id);
        m_pathRenderer->pathLineTo({x1, y, pos.z}, strokeSize, strokeColor, id);
        m_pathRenderer->pathLineTo({x1, y1, pos.z}, strokeSize, strokeColor, id);
        m_pathRenderer->pathLineTo({x, y1, pos.z}, strokeSize, ViewportTheme::colors.wire, id);
        m_pathRenderer->endPathMode(true, true, fillColor);

        const auto textSize = m_materialRenderer->getTextRenderSize(tagComp.name, componentStyles.headerFontSize);
        glm::vec3 textPos = {pos.x,
                             y + componentStyles.paddingY + strokeSize,
                             pos.z + 0.0005f};
        textPos.x -= textSize.x / 2.f;
        textPos.y += componentStyles.headerFontSize / 2.f;
        m_materialRenderer->drawText(tagComp.name, textPos, schematicCompStyles.nameFontSize, textColor, id, 0.f);

        {
            const auto compState = SimEngine::SimulationEngine::instance().getComponentState(simComp.simEngineEntity);
            auto tex = m_artistTools.sevenSegDispTexs[0];
            const auto texSize = tex->getScale();
            float texWidth = 60;
            float texHeight = (texSize.y / texSize.x) * texWidth;
            glm::vec3 texPos = {pos.x,
                                pos.y,
                                transform.position.z + 0.0001};

            m_materialRenderer->drawTexturedQuad(texPos, {texWidth, texHeight}, glm::vec4(1.f), static_cast<int>(entity), m_artistTools.sevenSegDispTexs[0]);

            texPos.z += 0.0001f;

            for (int i = 0; i < static_cast<int>(compState.inputStates.size()); i++) {
                if (!compState.inputStates[i])
                    continue;
                tex = m_artistTools.sevenSegDispTexs[i + 1];
                m_materialRenderer->drawTexturedQuad(texPos, {texWidth, texHeight}, glm::vec4(1.f), (int)entity, tex);
            }
        }
    }

    void SchematicArtist::paintSchematicView(entt::entity entity,
                                             const Components::TagComponent &tagComp,
                                             const Components::TransformComponent &transform,
                                             const Components::SpriteComponent &spriteComp,
                                             const Components::SimulationComponent &simComponent) {
        const auto &pos = transform.position;

        const auto schematicInfo = getCompSchematicInfo(entity);

        if (!schematicInfo.shouldDraw)
            return;

        const auto &textColor = ViewportTheme::schematicViewColors.text;
        const auto &fillColor = ViewportTheme::schematicViewColors.componentFill;
        const auto &strokeColor = ViewportTheme::schematicViewColors.componentStroke;

        float nodeWeight = schematicCompStyles.strokeSize;
        constexpr float negCircleR = schematicCompStyles.negCircleR;

        auto negateCircleAt = [&](const glm::vec3 pos) {
            m_materialRenderer->drawCircle(pos, negCircleR, strokeColor, -1, negCircleR - nodeWeight);
        };

        const int id = (int)entity;

        float w = schematicInfo.width;
        float h = schematicInfo.height;
        const float x = pos.x - (w / 2), x1 = pos.x + (w / 2);
        const float y = pos.y - (h / 2), y1 = pos.y + (h / 2);
        const float cpXL = x + (w * 0.25f);
        const float rb = schematicInfo.outPinStart;

        bool showName = true;

        auto itr = m_artistTools.schematicSymbolPaths.find(simComponent.defHash);
        if (itr != m_artistTools.schematicSymbolPaths.end()) {
            auto &diagram = itr->second;
            showName = diagram.showName();
            const auto strokeWeight = diagram.getStrokeSize() > 0.f
                                          ? diagram.getStrokeSize()
                                          : nodeWeight;

            ContoursDrawInfo info;
            info.fillColor = fillColor;
            info.strokeColor = strokeColor;
            info.glyphId = id;

            const glm::vec2 boxSize = glm::vec2(w, h);
            const glm::vec2 diagramSize = glm::vec2(diagram.getSize().x, diagram.getSize().y);

            if (diagramSize.x <= 0.0f || diagramSize.y <= 0.0f) {
                BESS_ERROR("Diagram has zero size");
                return;
            }

            const glm::vec2 scaleNonUniform = boxSize / diagramSize;
            glm::vec2 baseScale;
            glm::vec2 inset(0.0f);

            const float s = std::max(scaleNonUniform.x, scaleNonUniform.y);
            baseScale = glm::vec2(s, s);
            const glm::vec2 used = diagramSize * s;
            inset = (boxSize - used) * 0.5f;

            glm::vec2 boxTopLeft = glm::vec2(pos.x, pos.y) - boxSize * 0.5f;

            auto &paths = itr->second.getPathsMut();
            for (auto &p : paths) {
                const glm::vec2 pathBounds = glm::vec2(p.getBounds().x, p.getBounds().y);
                const glm::vec2 pathLowest = glm::vec2(p.getLowestPos().x, p.getLowestPos().y);

                glm::vec2 finalScale = baseScale;
                glm::vec2 finalPosPx = boxTopLeft + inset + pathLowest * finalScale;

                info.translate = {finalPosPx.x, finalPosPx.y, pos.z};
                info.scale = {finalScale.x, finalScale.y};

                h = std::max(h, pathBounds.y * finalScale.y);
                w = std::max(w, pathBounds.x * finalScale.x);

                p.setStrokeWidth(strokeWeight);
                m_pathRenderer->drawPath(p, info);
            }
        } else {
            m_pathRenderer->beginPathMode({x, y, pos.z}, nodeWeight, strokeColor, id);
            m_pathRenderer->pathLineTo({x1, y, pos.z}, nodeWeight, strokeColor, id);
            m_pathRenderer->pathLineTo({x1, y1, pos.z}, nodeWeight, strokeColor, id);
            m_pathRenderer->pathLineTo({x, y1, pos.z}, nodeWeight, ViewportTheme::colors.wire, id);
            m_pathRenderer->endPathMode(true, true, fillColor);
        }

        if (showName) {
            const auto textSize = m_materialRenderer->getTextRenderSize(tagComp.name, componentStyles.headerFontSize);
            glm::vec3 textPos = {pos.x, y + ((y1 - y) / 2.f), pos.z + 0.0005f};
            textPos.x -= textSize.x / 2.f;
            textPos.y += componentStyles.headerFontSize / 2.f;
            m_materialRenderer->drawText(tagComp.name, textPos, schematicCompStyles.nameFontSize, textColor, id, 0.f);
        }

        m_diagramSize[id] = glm::vec2(w, h);
    }

    void SchematicArtist::drawSlots(const entt::entity parentEntt, const Components::SimulationComponent &simComp, const Components::TransformComponent &transformComp) {
        const auto &def = SimEngine::SimulationEngine::instance().getComponentDefinition(simComp.simEngineEntity);
        const auto &[inpDetails, outDetails] = def.getPinDetails();

        const auto &size = m_diagramSize.at((int)parentEntt);
        const auto &schematicInfo = getCompSchematicInfo(parentEntt, size.x, size.y);
        const auto &schematicInfo_ = getCompSchematicInfo(parentEntt);

        float inPinStart = schematicInfo.inpPinStart;
        const float h = size.y;
        const auto &pos = transformComp.position;
        const float y = pos.y - (h / 2);

        const auto &pinColor = ViewportTheme::schematicViewColors.pin;
        constexpr float nodeWeight = schematicCompStyles.strokeSize;

        std::string label;
        // inputs
        {
            const size_t inpCount = simComp.inputSlots.size();
            const float yIncr = h / ((float)inpCount + 1);
            for (int i = 0; i < inpCount; i++) {
                float pinY = y + (yIncr * (float)(i + 1));
                const int pinId = static_cast<int>(Scene::instance()->getEntityWithUuid(simComp.inputSlots[i]));
                m_pathRenderer->beginPathMode({inPinStart, pinY, pos.z - 0.0005f}, nodeWeight, pinColor, pinId);
                m_pathRenderer->pathLineTo({schematicInfo.inpConnStart, pinY, pos.z - 0.0005f}, nodeWeight, pinColor, pinId);
                m_pathRenderer->endPathMode(false);
                label = inpDetails.size() > i ? inpDetails[i].name : "X" + std::to_string(i);
                m_materialRenderer->drawText(label,
                                             {schematicInfo.inpConnStart, pinY - nodeWeight, pos.z - 0.0005f},
                                             componentStyles.slotLabelSize, ViewportTheme::colors.text, static_cast<int>(parentEntt), 0.f);
            }
        }

        // outputs
        {
            const size_t outCount = simComp.outputSlots.size();
            const float yIncr = h / (float)(outCount + 1);
            for (int i = 0; i < outCount; i++) {
                float pinY = y + (yIncr * (float)(i + 1));
                const int pinId = static_cast<int>(Scene::instance()->getEntityWithUuid(simComp.outputSlots[i]));
                m_pathRenderer->beginPathMode({schematicInfo.outPinStart, pinY, pos.z - 0.0005f}, nodeWeight, pinColor, pinId);
                m_pathRenderer->pathLineTo({schematicInfo.outConnStart, pinY, pos.z - 0.0005f}, nodeWeight, pinColor, pinId);
                m_pathRenderer->endPathMode(false);
                label = outDetails.size() > i ? outDetails[i].name : "Y" + std::to_string(i);
                const float size = m_materialRenderer->getTextRenderSize(label, componentStyles.slotLabelSize).x;
                m_materialRenderer->drawText(label,
                                             {schematicInfo.outConnStart - size, pinY - nodeWeight, pos.z - 0.0005f},
                                             componentStyles.slotLabelSize, ViewportTheme::colors.text, static_cast<int>(parentEntt), 0.f);
            }
        }
    }

    ArtistCompSchematicInfo SchematicArtist::getCompSchematicInfo(entt::entity ent, float width, float height) const {
        const auto &reg = Scene::instance()->getEnttRegistry();
        const auto &tagComp = reg.get<Components::TagComponent>(ent);
        const auto &simComp = reg.get<Components::SimulationComponent>(ent);
        const auto &transform = reg.get<Components::TransformComponent>(ent);
        const auto &pos = transform.position;

        const size_t n = std::max(simComp.inputSlots.size(), simComp.outputSlots.size());
        const float h = height == -1.f
                            ? (SCHEMATIC_VIEW_PIN_ROW_SIZE * (float)n) + (schematicCompStyles.paddingY * 2.f)
                            : height;

        float w = width == -1.f ? h * 1.2f : width;
        float x = pos.x - (w / 2), x1 = pos.x + (w / 2);
        constexpr float negCircleOff = schematicCompStyles.negCircleOff;

        ArtistCompSchematicInfo info;

        info.outPinStart = x1;

        auto labelW = m_materialRenderer->getTextRenderSize(tagComp.name,
                                                            schematicCompStyles.nameFontSize)
                          .x +
                      (componentStyles.paddingX * 2.f);

        w = std::max(w, labelW);

        x = pos.x - (w / 2), x1 = pos.x + (w / 2);

        info.inpPinStart = x;
        info.outPinStart = x1;
        info.inpConnStart = info.inpPinStart - schematicCompStyles.pinSize;
        info.outConnStart = info.outPinStart + schematicCompStyles.pinSize;
        info.width = w;
        info.height = h;

        return info;
    }

    ArtistCompSchematicInfo SchematicArtist::getCompSchematicInfo(const UUID uuid) const {
        return getCompSchematicInfo(Scene::instance()->getEntityWithUuid(uuid));
    }

    glm::vec3 SchematicArtist::getSlotPos(const Components::SlotComponent &comp,
                                          const Components::TransformComponent &parentTransform) {
        auto &registry = Scene::instance()->getEnttRegistry();
        const auto parentEntt = Scene::instance()->getEntityWithUuid(comp.parentId);
        const auto &simComp = registry.get<Components::SimulationComponent>(parentEntt);

        const auto &size = m_diagramSize.at((int)parentEntt);
        const auto info = getCompSchematicInfo(parentEntt, size.x, size.y);

        float x = 0, y = parentTransform.position.y - (info.height / 2.f);

        const bool isOutputSlot = comp.slotType == Components::SlotType::digitalOutput;

        float yIncr = 0;
        if (isOutputSlot) {
            x = info.outConnStart;
            yIncr = info.height / (float)(simComp.outputSlots.size() + 1);
        } else {
            x = info.inpConnStart;
            yIncr = info.height / (float)(simComp.inputSlots.size() + 1);
        }

        y += yIncr * (float)(comp.idx + 1);

        return {x, y, parentTransform.position.z + 0.0005};
    }
} // namespace Bess::Canvas
