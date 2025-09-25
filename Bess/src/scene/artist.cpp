#include "scene/artist.h"
#include "asset_manager/asset_manager.h"
#include "assets.h"
#include "common/log.h"
#include "component_catalog.h"
#include "component_types/component_types.h"
#include "entt/entity/fwd.hpp"
#include "ext/vector_float3.hpp"
#include "scene/components/components.h"
#include "scene/renderer/renderer.h"
#include "scene/scene.h"
#include "settings/viewport_theme.h"
#include "simulation_engine.h"
#include "types.h"
#include <cstdint>
#include <string>
#include <vector>

using Renderer = Bess::Renderer2D::Renderer;

namespace Bess::Canvas {
    Scene *Artist::sceneRef = nullptr;

    constexpr struct ComponentStyles {
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

        inline float getSlotColumnSize() const {
            return (slotRadius * 2.f) + slotMargin + slotLabelSize;
        }
    } componentStyles;

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

    constexpr float SLOT_DX = componentStyles.paddingX + componentStyles.slotRadius + componentStyles.slotMargin;

    constexpr float SLOT_START_Y = componentStyles.headerHeight;

    constexpr float SLOT_ROW_SIZE = (componentStyles.rowMargin * 2.f) + (componentStyles.slotRadius * 2.f) + componentStyles.rowGap;

    constexpr float SLOT_COLUMN_SIZE = (componentStyles.slotRadius + componentStyles.slotMargin + componentStyles.slotLabelSize) * 2;

    constexpr float SCHEMATIC_VIEW_PIN_ROW_SIZE = schematicCompStyles.nameFontSize + schematicCompStyles.strokeSize + schematicCompStyles.pinRowGap;

    ArtistTools Artist::m_artistTools;
    ArtistInstructions Artist::m_instructions;

    void Artist::init() {
        {

            auto tex = Assets::AssetManager::instance().get(Assets::TileMaps::sevenSegDisplay);
            float margin = 4.f;
            glm::vec2 size(128.f, 234.f);
            m_artistTools.sevenSegDispTexs = std::array<std::shared_ptr<Gl::SubTexture>, 8>{
                std::make_shared<Gl::SubTexture>(tex, glm::vec2({0.f, 0.f}), size, margin, glm::vec2(1.f)),
                std::make_shared<Gl::SubTexture>(tex, glm::vec2({1.f, 0.f}), size, margin, glm::vec2(1.f)),
                std::make_shared<Gl::SubTexture>(tex, glm::vec2({2.f, 0.f}), size, margin, glm::vec2(1.f)),
                std::make_shared<Gl::SubTexture>(tex, glm::vec2({3.f, 0.f}), size, margin, glm::vec2(1.f)),
                std::make_shared<Gl::SubTexture>(tex, glm::vec2({4.f, 0.f}), size, margin, glm::vec2(1.f)),
                std::make_shared<Gl::SubTexture>(tex, glm::vec2({0.f, 1.f}), size, margin, glm::vec2(1.f)),
                std::make_shared<Gl::SubTexture>(tex, glm::vec2({1.f, 1.f}), size, margin, glm::vec2(1.f)),
                std::make_shared<Gl::SubTexture>(tex, glm::vec2({2.f, 1.f}), size, margin, glm::vec2(1.f)),
            };
        }
    }

    glm::vec3 Artist::getSlotPos(const Components::SlotComponent &comp, const Components::TransformComponent &parentTransform) {
        auto pPos = parentTransform.position;
        auto pScale = parentTransform.scale;

        auto slotdx = SLOT_DX;

        auto posX = pPos.x - pScale.x / 2.f;

        bool isOutputSlot = comp.slotType == Components::SlotType::digitalOutput;
        if (isOutputSlot) {
            slotdx *= -1;
            posX += pScale.x;
        }

        posX += slotdx;
        float posY = pPos.y - pScale.y / 2.f + (SLOT_ROW_SIZE * comp.idx) + SLOT_ROW_SIZE / 2.f;

        auto parentEntt = sceneRef->getEntityWithUuid(comp.parentId);
        const auto &isNonHeader = sceneRef->getEnttRegistry().any_of<Components::SimulationInputComponent, Components::SimulationOutputComponent, Components::SimulationStateMonitor>(parentEntt);
        if (!isNonHeader)
            posY += SLOT_START_Y;

        return glm::vec3(posX, posY, pPos.z + 0.0005);
    }

    glm::vec3 Artist::getPinPos(UUID uuid) {
        auto ent = sceneRef->getEntityWithUuid(uuid);
        const auto &reg = sceneRef->getEnttRegistry();
        const auto &comp = reg.get<Components::SlotComponent>(ent);
        return getPinPos(comp);
    }

    glm::vec3 Artist::getPinPos(const Components::SlotComponent &comp) {
        auto &registry = sceneRef->getEnttRegistry();
        auto parentEntt = sceneRef->getEntityWithUuid(comp.parentId);
        const auto &pTransform = registry.get<Components::TransformComponent>(parentEntt);
        const auto &simComp = registry.get<Components::SimulationComponent>(parentEntt);

        const auto &pScale = pTransform.scale;
        const auto info = getCompSchematicInfo(parentEntt);

        SimEngine::ComponentType type = SimEngine::SimulationEngine::instance().getComponentType(simComp.simEngineEntity);

        float x = 0, y = pTransform.position.y - info.height / 2.f;

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

        return {x, y, pTransform.position.z + 0.0005};
    }

    void Artist::paintSlot(uint64_t id, uint64_t parentId, const glm::vec3 &pos,
                           float angle, const std::string &label, float labelDx,
                           bool isHigh, bool isConnected, SimEngine::ExtendedPinType extendedType) {
        auto bg = ViewportTheme::colors.stateLow;
        auto border = ViewportTheme::colors.stateLow;
        if (extendedType == SimEngine::ExtendedPinType::inputClock) {
            if (isHigh) {
                bg = ViewportTheme::colors.clockConnectionHigh;
                border = ViewportTheme::colors.clockConnectionHigh;
            } else {
                bg = ViewportTheme::colors.clockConnectionLow;
                border = ViewportTheme::colors.clockConnectionLow;
            }

        } else if (isHigh) {
            bg = ViewportTheme::colors.stateHigh;
            border = ViewportTheme::colors.stateHigh;
        }

        if (!isConnected)
            bg.a = 0.1f;

        float ir = componentStyles.slotRadius - componentStyles.slotBorderSize;
        float r = componentStyles.slotRadius;

        // if (extendedType == SimEngine::ExtendedPinType::inputClock) {
        //     auto shift = (componentStyles.slotRadius);
        //     auto top = glm::vec3{pos.x - shift, pos.y - shift, pos.z};
        //     auto bottom = glm::vec3{pos.x - shift, pos.y + shift, pos.z};
        //     auto right = glm::vec3{pos.x + shift, pos.y, pos.z};
        //     auto ctrlPoint = glm::vec2{pos.x - 2.f, pos.y};
        //     float borderSize = componentStyles.slotBorderSize * 1.5f;
        //
        //     std::cout << borderSize << std::endl;
        //     Renderer::beginPathMode(top, borderSize, border, id);
        //     Renderer::pathQuadBeizerTo(bottom, ctrlPoint, borderSize, border, id);
        //     Renderer::pathLineTo(right, borderSize, border, id);
        //     Renderer::endPathMode(true, true, bg);
        // } else
        if (extendedType == SimEngine::ExtendedPinType::inputClear) {
            Renderer2D::QuadRenderProperties props;
            props.borderColor = border;
            props.borderRadius = glm::vec4(2.5f);
            props.borderSize = glm::vec4(componentStyles.slotBorderSize + 0.5);
            Renderer::quad(pos, glm::vec2(r * 2.f), glm::vec4(0.f), id, props);
            props.borderSize = {};
            props.borderRadius = glm::vec4(1.5f);
            Renderer::quad(pos, glm::vec2((ir - 1) * 2.f), glm::vec4(bg), id, props);
        } else {
            Renderer::circle(pos, r, border, id, ir);
            Renderer::circle(pos, ir - 1.f, bg, id);
        }

        float labelX = pos.x + labelDx;
        float dY = componentStyles.slotRadius - std::abs(componentStyles.slotRadius * 2.f - componentStyles.slotLabelSize) / 2.f;
        Renderer::msdfText(label, {labelX, pos.y + dY, pos.z}, componentStyles.slotLabelSize, ViewportTheme::colors.text, parentId, angle);
    }

    void Artist::drawSlots(const Components::SimulationComponent &comp, const Components::TransformComponent &transformComp) {
        auto def = SimEngine::ComponentCatalog::instance().getComponentDefinition(comp.type);
        auto &registry = sceneRef->getEnttRegistry();
        auto slotsView = registry.view<Components::SlotComponent>();

        float labeldx = componentStyles.slotMargin + (componentStyles.slotRadius * 2.f);

        auto compState = SimEngine::SimulationEngine::instance().getComponentState(comp.simEngineEntity);

        float angle = transformComp.angle;
        auto [inpDetails, outDetails] = def->getPinDetails();

        std::string label;
        for (size_t i = 0; i < comp.inputSlots.size(); i++) {
            auto slot = sceneRef->getEntityWithUuid(comp.inputSlots[i]);
            auto isHigh = compState.inputStates[i];
            auto isConnected = compState.inputConnected[i];
            auto &slotComp = slotsView.get<Components::SlotComponent>(slot);
            auto slotPos = getSlotPos(slotComp, transformComp);
            uint64_t parentId = (uint64_t)sceneRef->getEntityWithUuid(slotComp.parentId);
            label = inpDetails.size() > i ? inpDetails[i].name : "X" + std::to_string(i);
            paintSlot((uint64_t)slot, parentId, slotPos, angle, label, labeldx, (bool)isHigh, isConnected,
                      inpDetails.size() > i ? inpDetails[i].extendedType : SimEngine::ExtendedPinType::none);
        }

        for (size_t i = 0; i < comp.outputSlots.size(); i++) {
            auto slot = sceneRef->getEntityWithUuid(comp.outputSlots[i]);
            auto isHigh = compState.outputStates[i];
            auto isConnected = compState.outputConnected[i];
            auto &slotComp = slotsView.get<Components::SlotComponent>(slot);
            auto slotPos = getSlotPos(slotComp, transformComp);
            uint64_t parentId = (uint64_t)sceneRef->getEntityWithUuid(slotComp.parentId);
            label = outDetails.size() > i ? outDetails[i].name : "Y" + std::to_string(i);
            float labelWidth = Renderer::getMSDFTextRenderSize(label, componentStyles.slotLabelSize).x;
            paintSlot((uint64_t)slot, parentId, slotPos, angle, label, -labeldx - labelWidth, (bool)isHigh, isConnected,
                      outDetails.size() > i ? outDetails[i].extendedType : SimEngine::ExtendedPinType::none);
        }
    }

    void Artist::drawGhostConnection(const entt::entity &startEntity, const glm::vec2 pos) {
        auto &registry = sceneRef->getEnttRegistry();
        auto slotsView = registry.view<Components::SlotComponent, Components::TransformComponent, Components::SimulationComponent>();
        auto &slotComp = slotsView.get<Components::SlotComponent>(startEntity);
        auto parentEntt = sceneRef->getEntityWithUuid(slotComp.parentId);
        auto &parentTransform = slotsView.get<Components::TransformComponent>(parentEntt);
        auto startPos = Artist::getSlotPos(slotComp, parentTransform);
        startPos.z = 0.f;

        float ratio = slotComp.slotType == Components::SlotType::digitalInput ? 0.8f : 0.2f;
        auto midX = startPos.x + ((pos.x - startPos.x) * ratio);

        Renderer::beginPathMode(startPos, 2.f, ViewportTheme::colors.ghostWire, -1);
        Renderer::pathLineTo(glm::vec3(midX, startPos.y, 0.f), 2.f, ViewportTheme::colors.ghostWire, -1);
        Renderer::pathLineTo(glm::vec3(midX, pos.y, 0.f), 2.f, ViewportTheme::colors.ghostWire, -1);
        Renderer::pathLineTo(glm::vec3(pos, 0.f), 2.f, ViewportTheme::colors.ghostWire, -1);
        Renderer::endPathMode();
    }

    void Artist::drawConnection(const UUID &id, entt::entity inputEntity, entt::entity outputEntity, bool isSelected) {
        auto &registry = sceneRef->getEnttRegistry();
        auto connEntity = sceneRef->getEntityWithUuid(id);
        auto &connectionComponent = registry.get<Components::ConnectionComponent>(connEntity);

        const auto &inpSlotComp = registry.get<Components::SlotComponent>(inputEntity);
        const auto &outputSlotComp = registry.get<Components::SlotComponent>(outputEntity);
        const auto outParentEntt = sceneRef->getEntityWithUuid(outputSlotComp.parentId);
        const auto &outParentSimComp = registry.get<Components::SimulationComponent>(outParentEntt);

        glm::vec3 startPos, endPos;
        if (m_instructions.isSchematicView) {
            auto inpParent = sceneRef->getEntityWithUuid(inpSlotComp.parentId);

            auto shouldDrawInp = getCompSchematicInfo(inpParent).shouldDraw;
            auto shouldDrawOut = getCompSchematicInfo(outParentEntt).shouldDraw;

            if (!shouldDrawInp || !shouldDrawOut) {
                return;
            }

            startPos = Artist::getPinPos(registry.get<Components::SlotComponent>(inputEntity));
            endPos = Artist::getPinPos(outputSlotComp);
        } else {
            const auto &parentTransform = registry.get<Components::TransformComponent>(sceneRef->getEntityWithUuid(inpSlotComp.parentId));
            startPos = Artist::getSlotPos(inpSlotComp, parentTransform);
            const auto &endParentTransform = registry.get<Components::TransformComponent>(sceneRef->getEntityWithUuid(outputSlotComp.parentId));
            endPos = Artist::getSlotPos(outputSlotComp, endParentTransform);
        }

        startPos.z = 0.f;
        endPos.z = 0.f;

        glm::vec4 color;
        if (connectionComponent.useCustomColor) {
            auto &spriteComponent = registry.get<Components::SpriteComponent>(connEntity);
            color = spriteComponent.color;
        } else if (m_instructions.isSchematicView) {
            color = ViewportTheme::schematicViewColors.connection;
        } else {
            bool isHigh = (bool)SimEngine::SimulationEngine::instance().getComponentState(outParentSimComp.simEngineEntity).outputStates[outputSlotComp.idx];
            color = isHigh ? ViewportTheme::colors.stateHigh : ViewportTheme::colors.stateLow;
        }

        color = isSelected ? ViewportTheme::colors.selectedWire : color;

        auto connSegEntt = sceneRef->getEntityWithUuid(connectionComponent.segmentHead);
        auto connSegComp = registry.get<Components::ConnectionSegmentComponent>(connSegEntt);
        auto segId = connectionComponent.segmentHead;
        auto prevPos = startPos;

        static constexpr int wireSize = 2.f;
        static constexpr int hoveredSize = 3.f;

        bool isHovered = registry.all_of<Components::HoveredEntityComponent>(connSegEntt);

        Renderer::beginPathMode(startPos, isHovered ? hoveredSize : wireSize, color, (uint64_t)connSegEntt);
        while (segId != UUID::null) {
            glm::vec3 pos = endPos;
            auto newSegId = connSegComp.next;

            if (newSegId != UUID::null) {
                auto newSegEntt = sceneRef->getEntityWithUuid(newSegId);
                connSegComp = registry.get<Components::ConnectionSegmentComponent>(newSegEntt);
                pos = glm::vec3(connSegComp.pos, prevPos.z);
                if (pos.x == 0.f) {
                    pos.x = prevPos.x;
                    if (connSegComp.isTail())
                        pos.y = endPos.y;
                } else {
                    pos.y = prevPos.y;
                    if (connSegComp.isTail())
                        pos.x = endPos.x;
                }
            }

            auto segEntt = sceneRef->getEntityWithUuid(segId);
            bool isHovered = registry.all_of<Components::HoveredEntityComponent>(segEntt);
            auto size = isHovered ? hoveredSize : wireSize;
            auto offPos = pos;
            auto offSet = (prevPos.y <= pos.y) ? wireSize / 2.f : -wireSize / 2.f;
            Renderer::pathLineTo(offPos, size, color, (uint64_t)segEntt);
            offPos.z += 0.0001f;

            segId = newSegId;
            prevPos = pos;
        }
        Renderer::endPathMode();
    }

    void Artist::drawConnectionEntity(entt::entity entity) {
        auto &registry = sceneRef->getEnttRegistry();

        auto &connComp = registry.get<Components::ConnectionComponent>(entity);
        auto &idComp = registry.get<Components::IdComponent>(entity);
        bool isSelected = registry.all_of<Components::SelectedComponent>(entity);
        Artist::drawConnection(idComp.uuid, sceneRef->getEntityWithUuid(connComp.inputSlot), sceneRef->getEntityWithUuid(connComp.outputSlot), isSelected);
    }

    void Artist::drawHeaderLessComp(entt::entity entity,
                                    Components::TagComponent &tagComp,
                                    Components::TransformComponent &transform,
                                    Components::SpriteComponent &spriteComp,
                                    Components::SimulationComponent &simComp) {
        auto &registry = sceneRef->getEnttRegistry();

        auto labelSize = Renderer::getMSDFTextRenderSize(tagComp.name, componentStyles.headerFontSize);

        uint64_t id = (uint64_t)entity;
        auto pos = transform.position;
        auto rotation = transform.angle;
        auto scale = transform.scale;
        float columnSize = componentStyles.getSlotColumnSize();

        float xSize = labelSize.x;
        xSize += componentStyles.paddingX * 2.f;

        float labelXGapL = 0.f, labelXGapR = 0.f;
        float labelLOffset = componentStyles.paddingX;

        if (!simComp.inputSlots.empty()) {
            labelXGapL = 2.f;
            xSize += SLOT_COLUMN_SIZE + labelXGapL;
            labelLOffset += SLOT_COLUMN_SIZE + labelXGapL;
        }

        if (!simComp.outputSlots.empty()) {
            labelXGapR = 2.f;
            xSize += SLOT_COLUMN_SIZE + labelXGapR;
        }

        scale.x = xSize;

        scale.y = SLOT_ROW_SIZE * std::max(simComp.inputSlots.size(), simComp.outputSlots.size());
        transform.scale = scale;

        bool isSelected = registry.any_of<Components::SelectedComponent>(entity);
        auto border = isSelected ? ViewportTheme::colors.selectedComp : spriteComp.borderColor;

        Renderer2D::QuadRenderProperties props;
        props.borderRadius = spriteComp.borderRadius;
        props.borderColor = border;
        props.borderSize = spriteComp.borderSize;
        props.isMica = true;
        Renderer::quad(pos, glm::vec2(scale), spriteComp.color, id, props);

        glm::vec3 textPos = glm::vec3(
            pos.x - (scale.x / 2.f) + labelLOffset,
            pos.y + (componentStyles.headerFontSize / 2.f) - 1.f, pos.z + 0.0005f);

        auto name = tagComp.name;
        Renderer::msdfText(name, textPos, componentStyles.headerFontSize, ViewportTheme::colors.text, id);

        drawSlots(simComp, transform);
    }

    void Artist::paintSchematicView(entt::entity entity) {
        const auto &registry = sceneRef->getEnttRegistry();
        const auto &simComp = registry.get<Components::SimulationComponent>(entity);
        SimEngine::ComponentType type = simComp.type;

        const auto &transformComp = registry.get<Components::TransformComponent>(entity);
        const auto &pos = transformComp.position;
        const auto &scale = transformComp.scale;

        const auto &tagComp = registry.get<Components::TagComponent>(entity);

        auto schematicInfo = getCompSchematicInfo(entity);

        if (!schematicInfo.shouldDraw)
            return;

        const auto &pinColor = ViewportTheme::schematicViewColors.pin;
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

        float inPinStart = schematicInfo.inpPinStart;

        if (showName) {
            auto textSize = Renderer2D::Renderer::getMSDFTextRenderSize(tagComp.name, componentStyles.headerFontSize);
            glm::vec3 textPos = {pos.x, y + (y1 - y) / 2.f, pos.z + 0.0005f};
            textPos.x -= textSize.x / 2.f;
            textPos.y += componentStyles.headerFontSize / 2.f;
            Renderer::msdfText(tagComp.name, textPos, schematicCompStyles.nameFontSize, textColor, id, 0.f);
        }

        auto def = SimEngine::ComponentCatalog::instance().getComponentDefinition(type);
        auto [inpDetails, outDetails] = def->getPinDetails();

        std::string label = "";
        // inputs
        {
            size_t inpCount = simComp.inputSlots.size();
            float yIncr = h / (inpCount + 1);
            for (int i = 0; i < inpCount; i++) {
                float pinY = y + yIncr * (i + 1);
                Renderer::beginPathMode({inPinStart, pinY, pos.z - 0.0005f}, nodeWeight, pinColor, id);
                Renderer::pathLineTo({schematicInfo.inpConnStart, pinY, pos.z - 0.0005f}, nodeWeight, pinColor, id);
                Renderer::endPathMode(false);
                label = inpDetails.size() > i ? inpDetails[i].name : "X" + std::to_string(i);
                Renderer::msdfText(label,
                                   {schematicInfo.inpConnStart, pinY - nodeWeight, pos.z - 0.0005f},
                                   componentStyles.headerFontSize, ViewportTheme::colors.text, 0, 0.f);
            }
        }

        // outputs
        {
            size_t outCount = simComp.outputSlots.size();
            float yIncr = h / (outCount + 1);
            for (int i = 0; i < outCount; i++) {
                float pinY = y + yIncr * (i + 1);
                Renderer::beginPathMode({schematicInfo.outPinStart, pinY, pos.z - 0.0005f}, nodeWeight, pinColor, id);
                Renderer::pathLineTo({schematicInfo.outConnStart, pinY, pos.z - 0.0005f}, nodeWeight, pinColor, id);
                Renderer::endPathMode(false);
                label = outDetails.size() > i ? outDetails[i].name : "Y" + std::to_string(i);
                float size = Renderer2D::Renderer::getMSDFTextRenderSize(label, componentStyles.headerFontSize).x;
                Renderer::msdfText(label,
                                   {schematicInfo.outConnStart - size, pinY - nodeWeight, pos.z - 0.0005f},
                                   componentStyles.headerFontSize, ViewportTheme::colors.text, 0, 0.f);
            }
        }
    }

    void Artist::drawNonSimEntity(entt::entity entity) {
        auto &registry = sceneRef->getEnttRegistry();
        const auto &tagComp = registry.get<Components::TagComponent>(entity);
        bool isSelected = registry.any_of<Components::SelectedComponent>(entity);

        uint64_t id = (uint64_t)entity;

        switch (tagComp.type.nsCompType) {
        case Components::NSComponentType::text: {
            auto &textComp = registry.get<Components::TextNodeComponent>(entity);
            auto &transformComp = registry.get<Components::TransformComponent>(entity);
            auto pos = transformComp.position;
            auto rotation = transformComp.angle;
            auto scale = transformComp.scale;
            Renderer::msdfText(textComp.text, pos, textComp.fontSize, textComp.color, id, rotation);

            if (isSelected) {
                Renderer2D::QuadRenderProperties props{
                    .angle = rotation,
                    .borderColor = ViewportTheme::colors.selectedComp,
                    .borderRadius = glm::vec4(4.f),
                    .borderSize = glm::vec4(1.f),
                    .isMica = true,
                };
                auto size = Renderer::getMSDFTextRenderSize(textComp.text, textComp.fontSize);
                pos.x += size.x * 0.5f;
                pos.y -= size.y * 0.25f;
                size.x += componentStyles.paddingX * 2.f;
                size.y += componentStyles.paddingY * 2.f;
                Renderer::quad(pos, size, ViewportTheme::colors.componentBG, id, props);
            }
        } break;
        default:
            BESS_ERROR("[Artist] Tried to draw a non-simulation component with type: {}", tagComp.type.typeId);
            break;
        }
    }

    void Artist::drawSevenSegDisplay(
        entt::entity entity,
        Components::TagComponent &tagComp,
        Components::TransformComponent &transform,
        Components::SpriteComponent &spriteComp,
        Components::SimulationComponent &simComp) {

        auto pos = transform.position;
        auto rotation = transform.angle;
        auto scale = transform.scale;
        int maxRows = std::max(simComp.inputSlots.size(), simComp.outputSlots.size());
        scale.y = componentStyles.headerHeight + componentStyles.rowGap + (maxRows * SLOT_ROW_SIZE);
        scale.x = 150.f;
        transform.scale = scale;

        float headerHeight = componentStyles.headerHeight;
        auto headerPos = glm::vec3(pos.x, pos.y - scale.y / 2.f + headerHeight / 2.f, pos.z);

        auto &registry = sceneRef->getEnttRegistry();
        bool isSelected = registry.any_of<Components::SelectedComponent>(entity);
        auto border = isSelected ? ViewportTheme::colors.selectedComp : spriteComp.borderColor;

        uint64_t id = (uint64_t)entity;

        glm::vec3 textPos = glm::vec3(pos.x - scale.x / 2.f + componentStyles.paddingX, headerPos.y + componentStyles.paddingY, pos.z + 0.0005f);

        Renderer2D::QuadRenderProperties props;
        props = {};
        props.angle = rotation;
        props.borderRadius = spriteComp.borderRadius;
        props.borderSize = spriteComp.borderSize;
        props.borderColor = border;
        props.isMica = true;

        Renderer::quad(pos, glm::vec2(scale), spriteComp.color, id, props);

        props = {};
        props.angle = rotation;
        props.borderSize = glm::vec4(0.f);
        props.borderRadius = glm::vec4(0, 0, spriteComp.borderRadius.x - spriteComp.borderSize.x, spriteComp.borderRadius.y - spriteComp.borderSize.y);
        props.isMica = true;

        Renderer::quad(headerPos,
                       glm::vec2(scale.x - spriteComp.borderSize.w - spriteComp.borderSize.y, headerHeight - spriteComp.borderSize.x - spriteComp.borderSize.z),
                       spriteComp.headerColor,
                       id,
                       props);

        Renderer::msdfText(tagComp.name, textPos, componentStyles.headerFontSize, ViewportTheme::colors.text, id, rotation);

        {
            auto compState = SimEngine::SimulationEngine::instance().getComponentState(simComp.simEngineEntity);
            auto tex = m_artistTools.sevenSegDispTexs[0];
            auto texSize = tex->getScale();
            float texWidth = 64;
            float texHeight = (texSize.y / texSize.x) * texWidth;
            Renderer::quad(transform.position + glm::vec3(24.f, 0.f, 0.f), {texWidth, texHeight}, tex, glm::vec4(1.f), (uint64_t)entity);

            for (int i = 0; i < (int)compState.inputStates.size(); i++) {
                if (!compState.inputStates[i])
                    continue;
                tex = m_artistTools.sevenSegDispTexs[i + 1];
                Renderer::quad(transform.position + glm::vec3(24.f, 0.f, 0.f), {texWidth, texHeight}, tex, glm::vec4(1.f), (uint64_t)entity);
            }
        }
        drawSlots(simComp, transform);
    }

    void Artist::drawSimEntity(
        entt::entity entity,
        Components::TagComponent &tagComp,
        Components::TransformComponent &transform,
        Components::SpriteComponent &spriteComp,
        Components::SimulationComponent &simComp) {
        auto &registry = sceneRef->getEnttRegistry();

        if (tagComp.isSimComponent && tagComp.type.simCompType == SimEngine::ComponentType::SEVEN_SEG_DISPLAY) {
            drawSevenSegDisplay(entity, tagComp, transform, spriteComp, simComp);
            return;
        }

        if (!m_instructions.isSchematicView) {
            if (tagComp.type.simCompType == SimEngine::ComponentType::INPUT || tagComp.type.simCompType == SimEngine::ComponentType::OUTPUT || tagComp.type.simCompType == SimEngine::ComponentType::STATE_MONITOR) {
                drawHeaderLessComp(entity, tagComp, transform, spriteComp, simComp);
                return;
            }
        }

        auto pos = transform.position;
        auto rotation = transform.angle;
        auto scale = transform.scale;
        int maxRows = std::max(simComp.inputSlots.size(), simComp.outputSlots.size());
        scale.y = componentStyles.headerHeight + componentStyles.rowGap + (maxRows * SLOT_ROW_SIZE);
        transform.scale = scale;

        if (m_instructions.isSchematicView) {
            paintSchematicView(entity);
            return;
        }

        float headerHeight = componentStyles.headerHeight;
        auto headerPos = glm::vec3(pos.x, pos.y - scale.y / 2.f + headerHeight / 2.f, pos.z);

        /*spriteComp.borderRadius = glm::vec4(radius);*/
        bool isSelected = registry.any_of<Components::SelectedComponent>(entity);
        auto border = isSelected ? ViewportTheme::colors.selectedComp : spriteComp.borderColor;

        uint64_t id = (uint64_t)entity;

        glm::vec3 textPos = glm::vec3(pos.x - scale.x / 2.f + componentStyles.paddingX, headerPos.y + componentStyles.paddingY, pos.z + 0.0005f);

        Renderer2D::QuadRenderProperties props;
        props.angle = rotation;
        props.borderRadius = spriteComp.borderRadius;
        props.borderSize = spriteComp.borderSize;
        props.borderColor = border;
        props.isMica = true;
        props.hasShadow = true;

        Renderer::quad(pos, glm::vec2(scale), spriteComp.color, id, props);

        props = {};
        props.angle = rotation;
        props.borderSize = glm::vec4(0.f);
        props.borderRadius = glm::vec4(0, 0, spriteComp.borderRadius.x - spriteComp.borderSize.x, spriteComp.borderRadius.y - spriteComp.borderSize.y);
        props.isMica = true;

        Renderer::quad(headerPos,
                       glm::vec2(scale.x - spriteComp.borderSize.w - spriteComp.borderSize.y, headerHeight - spriteComp.borderSize.x - spriteComp.borderSize.z),
                       spriteComp.headerColor,
                       id,
                       props);

        Renderer::msdfText(tagComp.name, textPos, componentStyles.headerFontSize, ViewportTheme::colors.text, id, rotation);

        drawSlots(simComp, transform);
    }

    ArtistCompSchematicInfo Artist::getCompSchematicInfo(entt::entity ent) {
        const auto &reg = sceneRef->getEnttRegistry();
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
            w = Renderer::getMSDFTextRenderSize(tagComp.name, schematicCompStyles.nameFontSize).x + componentStyles.paddingX * 2.f;
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

    void Artist::setInstructions(const ArtistInstructions &value) {
        m_instructions = value;
    }
} // namespace Bess::Canvas
