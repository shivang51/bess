#include "scene/artist.h"
#include "assets.h"
#include "common/log.h"
#include "entt/entity/fwd.hpp"
#include "ext/vector_float3.hpp"
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
    bool Artist::m_isSchematicMode = false;

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

    constexpr float SLOT_DX = componentStyles.paddingX + componentStyles.slotRadius + componentStyles.slotMargin;

    constexpr float SLOT_START_Y = componentStyles.headerHeight;

    constexpr float SLOT_ROW_SIZE = (componentStyles.rowMargin * 2.f) + (componentStyles.slotRadius * 2.f) + componentStyles.rowGap;

    constexpr float SLOT_COLUMN_SIZE = (componentStyles.slotRadius + componentStyles.slotMargin + componentStyles.slotLabelSize) * 2;

    ArtistTools Artist::m_artistTools;

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

        {

            auto tex = Assets::AssetManager::instance().get(Assets::TileMaps::sceneIcons);
            float margin = 5.f;
            glm::vec2 size(60.f, 60.f);
            m_artistTools.clockSlotsTexs = std::array<std::shared_ptr<Gl::SubTexture>, 2>{
                std::make_shared<Gl::SubTexture>(tex, glm::vec2({0.f, 0.f}), size, margin, glm::vec2(1.f)),
                std::make_shared<Gl::SubTexture>(tex, glm::vec2({1.f, 0.f}), size, margin, glm::vec2(1.f)),
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

    glm::vec3 Artist::getPinPos(const Components::SlotComponent &comp) {
        auto &registry = sceneRef->getEnttRegistry();
        auto parentEntt = sceneRef->getEntityWithUuid(comp.parentId);
        const auto &pTransform = registry.get<Components::TransformComponent>(parentEntt);
        auto &simComp = registry.get<Components::SimulationComponent>(parentEntt);

        auto pScale = pTransform.scale;
        auto pPos = pTransform.position - glm::vec3(pScale.x * 0.25f, pScale.y * 0.5f, 0.f);
        SimEngine::ComponentType type = SimEngine::SimulationEngine::instance().getComponentType(simComp.simEngineEntity);

        float x = 0, y = 0;

        bool isOutputSlot = comp.slotType == Components::SlotType::digitalOutput;
        auto info = getCompBoundInfo(type, pTransform.position, pTransform.scale);

        float yIncr = 0;
        if (isOutputSlot) {
            x = info.outConnStart;
            yIncr = pScale.y / (simComp.outputSlots.size() + 1);
        } else {
            x = info.inpConnStart;
            yIncr = pScale.y / (simComp.inputSlots.size() + 1);
        }

        y = pPos.y + yIncr * (comp.idx + 1);

        return {x, y, pPos.z + 0.0005};
    }

    void Artist::paintSlot(uint64_t id, uint64_t parentId, const glm::vec3 &pos,
                           float angle, const std::string &label, float labelDx,
                           bool isHigh, bool isConnected, bool isClock) {
        auto bgColor = ViewportTheme::stateLowColor;
        auto borderColor = ViewportTheme::stateLowColor;

        if (isHigh) {
            bgColor = ViewportTheme::stateHighColor;
            borderColor = ViewportTheme::stateHighColor;
        }

        if (!isConnected)
            bgColor.a = 0.1f;

        float ir = componentStyles.slotRadius - componentStyles.slotBorderSize;
        float r = componentStyles.slotRadius;

        // if (isClock) {
        //     if (!isConnected)
        //         Renderer::quad(pos, glm::vec2(r * 2.f), m_artistTools.clockSlotsTexs[0], borderColor, id, {});
        //     else
        //         Renderer::quad(pos, glm::vec2(r * 2.f), m_artistTools.clockSlotsTexs[1], borderColor, id, {});
        // } else {
        Renderer::circle(pos, r, borderColor, id, ir);
        Renderer::circle(pos, ir - 1.f, bgColor, id);
        // }

        float labelX = pos.x + labelDx;
        float dY = componentStyles.slotRadius - std::abs(componentStyles.slotRadius * 2.f - componentStyles.slotLabelSize) / 2.f;
        Renderer::msdfText(label, {labelX, pos.y + dY, pos.z}, componentStyles.slotLabelSize, ViewportTheme::textColor, parentId, angle);
    }

    void Artist::drawSlots(const Components::SimulationComponent &comp, const Components::TransformComponent &transformComp, const SimEngine::ComponentType compType) {
        auto &registry = sceneRef->getEnttRegistry();
        auto slotsView = registry.view<Components::SlotComponent>();

        float labeldx = componentStyles.slotMargin + (componentStyles.slotRadius * 2.f);

        auto compState = SimEngine::SimulationEngine::instance().getComponentState(comp.simEngineEntity);

        float angle = transformComp.angle;

        bool hasClockPin = compType == SimEngine::ComponentType::FLIP_FLOP_JK;

        std::string label;
        for (size_t i = 0; i < comp.inputSlots.size(); i++) {
            auto slot = sceneRef->getEntityWithUuid(comp.inputSlots[i]);
            auto isHigh = compState.inputStates[i];
            auto isConnected = compState.inputConnected[i];
            auto &slotComp = slotsView.get<Components::SlotComponent>(slot);
            auto slotPos = getSlotPos(slotComp, transformComp);
            uint64_t parentId = (uint64_t)sceneRef->getEntityWithUuid(slotComp.parentId);
            label = "X" + std::to_string(i);
            paintSlot((uint64_t)slot, parentId, slotPos, angle, label, labeldx, (bool)isHigh, isConnected, hasClockPin && i == 1);
        }

        float labelWidth = Renderer::getMSDFTextRenderSize("Y0", componentStyles.slotLabelSize).x;
        labeldx += labelWidth;
        for (size_t i = 0; i < comp.outputSlots.size(); i++) {
            auto slot = sceneRef->getEntityWithUuid(comp.outputSlots[i]);
            auto isHigh = compState.outputStates[i];
            auto isConnected = compState.outputConnected[i];
            auto &slotComp = slotsView.get<Components::SlotComponent>(slot);
            auto slotPos = getSlotPos(slotComp, transformComp);
            uint64_t parentId = (uint64_t)sceneRef->getEntityWithUuid(slotComp.parentId);
            label = "Y" + std::to_string(i);
            paintSlot((uint64_t)slot, parentId, slotPos, angle, label, -labeldx, (bool)isHigh, isConnected);
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

        std::vector<glm::vec3> points;
        points.emplace_back(startPos);
        points.emplace_back(glm::vec3(midX, startPos.y, 0.f));
        points.emplace_back(glm::vec3(midX, pos.y, 0.f));
        points.emplace_back(glm::vec3(pos, 0.f));

        Renderer::drawLines(points, 2.f, ViewportTheme::wireColor, -1);
    }

    void Artist::drawConnection(const UUID &id, entt::entity inputEntity, entt::entity outputEntity, bool isSelected) {
        auto &registry = sceneRef->getEnttRegistry();
        auto connEntity = sceneRef->getEntityWithUuid(id);
        auto &connectionComponent = registry.get<Components::ConnectionComponent>(connEntity);
        auto &outputSlotComp = registry.get<Components::SlotComponent>(outputEntity);
        auto &simComp = registry.get<Components::SimulationComponent>(sceneRef->getEntityWithUuid(outputSlotComp.parentId));

        glm::vec3 startPos, endPos;
        if (m_isSchematicMode) {
            startPos = Artist::getPinPos(registry.get<Components::SlotComponent>(inputEntity));
            endPos = Artist::getPinPos(outputSlotComp);
        } else {
            const auto &inpSlotComp = registry.get<Components::SlotComponent>(inputEntity);
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
        } else {
            bool isHigh = (bool)SimEngine::SimulationEngine::instance().getComponentState(simComp.simEngineEntity).outputStates[outputSlotComp.idx];
            color = isHigh ? ViewportTheme::stateHighColor : ViewportTheme::stateLowColor;
        }
        color = isSelected ? ViewportTheme::selectedWireColor : color;

        auto connSegEntt = sceneRef->getEntityWithUuid(connectionComponent.segmentHead);
        auto connSegComp = registry.get<Components::ConnectionSegmentComponent>(connSegEntt);
        auto segId = connectionComponent.segmentHead;
        auto prevPos = startPos;

        static int wireSize = 2.f;
        static int hoveredSize = 3.f;

        while (segId != UUID::null) {
            glm::vec3 pos = endPos;
            auto newSegId = connSegComp.next;

            if (newSegId != UUID::null) {
                auto newSegEntt = sceneRef->getEntityWithUuid(newSegId);
                connSegComp = registry.get<Components::ConnectionSegmentComponent>(newSegEntt);
                pos = glm::vec3(connSegComp.pos, prevPos.z);
                if (pos.x == 0.f) {
                    pos.x = prevPos.x;
                    if (connSegComp.isTail()) // for leveling with the end pos
                        pos.y = endPos.y;
                } else {
                    pos.y = prevPos.y;
                    if (connSegComp.isTail()) // for leveling with the end pos
                        pos.x = endPos.x;
                }
            }

            auto segEntt = sceneRef->getEntityWithUuid(segId);
            bool isHovered = registry.all_of<Components::HoveredEntityComponent>(segEntt);
            auto size = isHovered ? hoveredSize : wireSize;
            auto offPos = pos;
            auto offSet = (prevPos.y <= pos.y) ? wireSize / 2.f : -wireSize / 2.f;
            Renderer::line(prevPos, offPos, size, color, (uint64_t)segEntt);
            offPos.z += 0.0001f;

            if (newSegId != UUID::null) {
                // circle at the join
                Renderer::circle(offPos, size * 0.5f, color, id);
            }

            segId = newSegId;
            prevPos = pos;
        }
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
        auto borderColor = isSelected ? ViewportTheme::selectedCompColor : spriteComp.borderColor;

        Renderer2D::QuadRenderProperties props;
        props.borderRadius = spriteComp.borderRadius;
        props.borderColor = borderColor;
        props.borderSize = spriteComp.borderSize;
        props.isMica = true;
        Renderer::quad(pos, glm::vec2(scale), spriteComp.color, id, props);

        glm::vec3 textPos = glm::vec3(
            pos.x - (scale.x / 2.f) + labelLOffset,
            pos.y + (componentStyles.headerFontSize / 2.f) - 1.f, pos.z + 0.0005f);

        auto name = tagComp.name;
        Renderer::msdfText(name, textPos, componentStyles.headerFontSize, ViewportTheme::textColor, id);

        drawSlots(simComp, transform, tagComp.type.simCompType);
    }

    void Artist::paintSchematicView(entt::entity entity) {
        auto &registry = sceneRef->getEnttRegistry();
        auto &simComp = registry.get<Components::SimulationComponent>(entity);
        SimEngine::ComponentType type = SimEngine::SimulationEngine::instance().getComponentType(simComp.simEngineEntity);
        const auto &transformComp = registry.get<Components::TransformComponent>(entity);
        auto pos = transformComp.position;
        auto scale = transformComp.scale;

        static float nodeWeight = 2.f;
        static float negCircleR = 4.f;

        auto boundInfo = getCompBoundInfo(type, pos, scale);
        float w = scale.x / 2, h = scale.y;
        float x = pos.x - w / 2, x1 = pos.x + w / 2;
        float y = pos.y - h / 2, y1 = pos.y + h / 2;
        float rb = boundInfo.outPinStart;

        auto negateCircleAt = [&](glm::vec3 pos) {
            Renderer::circle(pos, negCircleR, ViewportTheme::compHeaderColor, -1, negCircleR - nodeWeight);
        };

        switch (type) {
        case SimEngine::ComponentType::AND:
        case SimEngine::ComponentType::NAND: {
            float cpX = x1 + (w * 0.65);
            // diagram
            Renderer::beginPathMode({x, y, pos.z}, nodeWeight, ViewportTheme::compHeaderColor);
            Renderer::pathLineTo({x1, y, pos.z}, 4.f, ViewportTheme::compHeaderColor, -1);
            Renderer::pathQuadBeizerTo({x1, y1, pos.z}, {cpX, y + (y1 - y) / 2}, 4, ViewportTheme::compHeaderColor, -1);
            Renderer::pathLineTo({x, y1, pos.z}, 4.f, ViewportTheme::wireColor, -1);
            Renderer::endPathMode(true);
        } break;
        case SimEngine::ComponentType::OR:
        case SimEngine::ComponentType::NOR: {
            float cpX = x + (w * 0.65);
            float off = w * 0.25;

            // diagram
            Renderer::beginPathMode({x, y, pos.z}, nodeWeight, ViewportTheme::compHeaderColor);
            Renderer::pathQuadBeizerTo({x, y1, pos.z}, {cpX, y + (y1 - y) / 2}, 4, ViewportTheme::compHeaderColor, -1);
            Renderer::pathLineTo({x1 - off, y1, pos.z}, 4.f, ViewportTheme::compHeaderColor, -1);
            Renderer::pathQuadBeizerTo({x1 + off, y + (y1 - y) / 2.f, pos.z}, {x1 + off * 0.45, y + (y1 - y) * 0.85}, 4.f, ViewportTheme::compHeaderColor, -1);
            Renderer::pathQuadBeizerTo({x1 - off, y, pos.z}, {x1 + off * 0.45, y + (y1 - y) * 0.15}, 4.f, ViewportTheme::compHeaderColor, -1);
            Renderer::endPathMode(true);
        } break;
        case SimEngine::ComponentType::XNOR:
        case SimEngine::ComponentType::XOR: {
            float cpX = x + (w * 0.65);
            float off = w * 0.25;

            // diagram
            Renderer::beginPathMode({x, y, pos.z}, nodeWeight, ViewportTheme::compHeaderColor);
            Renderer::pathQuadBeizerTo({x, y1, pos.z}, {cpX, y + (y1 - y) / 2}, 4, ViewportTheme::compHeaderColor, -1);
            Renderer::endPathMode(false);
            float gapX = 8.f;
            Renderer::beginPathMode({x + gapX, y, pos.z}, nodeWeight, ViewportTheme::compHeaderColor);
            Renderer::pathQuadBeizerTo({x + gapX, y1, pos.z}, {cpX + gapX, y + (y1 - y) / 2}, 4, ViewportTheme::compHeaderColor, -1);
            Renderer::pathLineTo({x1 - off, y1, pos.z}, 4.f, ViewportTheme::compHeaderColor, -1);
            Renderer::pathQuadBeizerTo({x1 + off, y + (y1 - y) / 2.f, pos.z}, {x1 + off * 0.45, y + (y1 - y) * 0.85}, 4.f, ViewportTheme::compHeaderColor, -1);
            Renderer::pathQuadBeizerTo({x1 - off, y, pos.z}, {x1 + off * 0.45, y + (y1 - y) * 0.15}, 4.f, ViewportTheme::compHeaderColor, -1);
            Renderer::endPathMode(true);
        } break;
        case SimEngine::ComponentType::NOT: {
            Renderer::beginPathMode({x, y, pos.z}, nodeWeight, ViewportTheme::compHeaderColor);
            Renderer::pathLineTo({x1, y + (y1 - y) / 2.f, pos.z}, 4.f, ViewportTheme::compHeaderColor, -1);
            Renderer::pathLineTo({x, y1, pos.z}, 4.f, ViewportTheme::compHeaderColor, -1);
            Renderer::endPathMode(true);
        } break;
        default:
            w = scale.x, h = scale.y;
            x = pos.x - w / 2, x1 = pos.x + w / 2;
            y = pos.y - h / 2, y1 = pos.y + h / 2;
            // a square with name in center
            Renderer::beginPathMode({x, y, pos.z}, nodeWeight, ViewportTheme::compHeaderColor);
            Renderer::pathLineTo({x1, y, pos.z}, 4.f, ViewportTheme::compHeaderColor, -1);
            Renderer::pathLineTo({x1, y1, pos.z}, 4.f, ViewportTheme::compHeaderColor, -1);
            Renderer::pathLineTo({x, y1, pos.z}, 4.f, ViewportTheme::wireColor, -1);
            Renderer::endPathMode(true);
            break;
        }

        // circle for negating components
        if (type == SimEngine::ComponentType::NAND || type == SimEngine::ComponentType::NOR || type == SimEngine::ComponentType::NOT || type == SimEngine::ComponentType::XNOR) {
            negateCircleAt({rb - negCircleR, y + (y1 - y) / 2.f, pos.z});
        }

        float inPinStart = boundInfo.inpPinStart; // need to be set node specific for some (its always right side of the pin)
        // name
        {
            const auto &tagComp = registry.get<Components::TagComponent>(entity);
            auto textSize = Renderer2D::Renderer::getMSDFTextRenderSize(tagComp.name, componentStyles.headerFontSize);
            glm::vec3 textPos = {inPinStart + (rb - inPinStart) / 2.f, y + (y1 - y) / 2.f, pos.z + 0.0005f};
            textPos.x -= textSize.x / 2.f;
            textPos.y += componentStyles.headerFontSize / 2.f;
            Renderer::msdfText(tagComp.name, textPos, componentStyles.headerFontSize, ViewportTheme::textColor, 0, 0.f);
        }

        // inputs
        {
            size_t inpCount = simComp.inputSlots.size();
            float yIncr = h / (inpCount + 1);
            for (int i = 1; i <= inpCount; i++) {
                float yOff = yIncr * i;
                Renderer::beginPathMode({inPinStart, y + yOff, pos.z}, nodeWeight, ViewportTheme::compHeaderColor);
                Renderer::pathLineTo({boundInfo.inpConnStart, y + yOff, 1}, 4.f, ViewportTheme::compHeaderColor, -1);
                Renderer::endPathMode(false);
                Renderer::msdfText("X" + std::to_string(i - 1),
                                   {boundInfo.inpConnStart, y + yOff - nodeWeight, pos.z + 0.0005f},
                                   componentStyles.headerFontSize, ViewportTheme::textColor, 0, 0.f);
            }
        }

        // outputs
        {
            size_t outCount = simComp.outputSlots.size();
            float yIncr = h / (outCount + 1);
            for (int i = 1; i <= outCount; i++) {
                float yOff = yIncr * i;
                Renderer::beginPathMode({boundInfo.outPinStart, y + yOff, pos.z}, nodeWeight, ViewportTheme::compHeaderColor);
                Renderer::pathLineTo({boundInfo.outConnStart, y + yOff, 1}, 4.f, ViewportTheme::compHeaderColor, -1);
                Renderer::endPathMode(false);
                std::string label = "Y" + std::to_string(i - 1);
                float size = Renderer2D::Renderer::getMSDFTextRenderSize(label, componentStyles.headerFontSize).x;
                Renderer::msdfText(label,
                                   {boundInfo.outConnStart - size, y + yOff - nodeWeight, pos.z + 0.0005f},
                                   componentStyles.headerFontSize, ViewportTheme::textColor, 0, 0.f);
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
                    .borderColor = ViewportTheme::selectedCompColor,
                    .borderRadius = glm::vec4(4.f),
                    .borderSize = glm::vec4(1.f),
                    .isMica = true,
                };
                auto size = Renderer::getMSDFTextRenderSize(textComp.text, textComp.fontSize);
                pos.x += size.x * 0.5f;
                pos.y -= size.y * 0.25f;
                size.x += componentStyles.paddingX * 2.f;
                size.y += componentStyles.paddingY * 2.f;
                Renderer::quad(pos, size, ViewportTheme::componentBGColor, id, props);
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
        auto borderColor = isSelected ? ViewportTheme::selectedCompColor : spriteComp.borderColor;

        uint64_t id = (uint64_t)entity;

        glm::vec3 textPos = glm::vec3(pos.x - scale.x / 2.f + componentStyles.paddingX, headerPos.y + componentStyles.paddingY, pos.z + 0.0005f);

        Renderer2D::QuadRenderProperties props;
        props = {};
        props.angle = rotation;
        props.borderRadius = spriteComp.borderRadius;
        props.borderSize = spriteComp.borderSize;
        props.borderColor = borderColor;
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

        Renderer::msdfText(tagComp.name, textPos, componentStyles.headerFontSize, ViewportTheme::textColor, id, rotation);

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
        drawSlots(simComp, transform, tagComp.type.simCompType);
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

        if (!m_isSchematicMode) {
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

        if (m_isSchematicMode) {
            paintSchematicView(entity);
            return;
        }

        float headerHeight = componentStyles.headerHeight;
        auto headerPos = glm::vec3(pos.x, pos.y - scale.y / 2.f + headerHeight / 2.f, pos.z);

        /*spriteComp.borderRadius = glm::vec4(radius);*/
        bool isSelected = registry.any_of<Components::SelectedComponent>(entity);
        auto borderColor = isSelected ? ViewportTheme::selectedCompColor : spriteComp.borderColor;

        uint64_t id = (uint64_t)entity;

        glm::vec3 textPos = glm::vec3(pos.x - scale.x / 2.f + componentStyles.paddingX, headerPos.y + componentStyles.paddingY, pos.z + 0.0005f);

        Renderer2D::QuadRenderProperties props;
        props.angle = rotation;
        props.borderRadius = spriteComp.borderRadius;
        props.borderSize = spriteComp.borderSize;
        props.borderColor = borderColor;
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

        Renderer::msdfText(tagComp.name, textPos, componentStyles.headerFontSize, ViewportTheme::textColor, id, rotation);

        drawSlots(simComp, transform, tagComp.type.simCompType);
    }

    ArtistCompBoundInfo Artist::getCompBoundInfo(SimEngine::ComponentType type, glm::vec2 pos, glm::vec2 scale) {
        float w = scale.x / 2, h = scale.y;
        float x = pos.x - w / 2, x1 = pos.x + w / 2;
        float y = pos.y - h / 2, y1 = pos.y + h / 2;
        static float pinW = 20.f;
        static float negCircleR = 4.f, negCircleOff = negCircleR * 2.f;
        ArtistCompBoundInfo info;

        switch (type) {
        case SimEngine::ComponentType::AND: {
            info.inpPinStart = x;
            info.outPinStart = x1 + ((w * 0.65) / 2);
            info.inpConnStart = info.inpPinStart - 20.f;
            info.outConnStart = info.outPinStart + 20.f;
        } break;
        case SimEngine::ComponentType::NAND: {
            info.inpPinStart = x;
            info.outPinStart = x1 + ((w * 0.65) / 2) + negCircleOff;
            info.inpConnStart = info.inpPinStart - 20.f;
            info.outConnStart = info.outPinStart + 20.f;
        } break;
        case SimEngine::ComponentType::XOR:
        case SimEngine::ComponentType::OR: {
            info.inpPinStart = x + (w * 0.55) / 2.f;
            info.outPinStart = x1 + w * 0.25;
            info.inpConnStart = x - pinW;
            info.outConnStart = info.outPinStart + 20.f;
        } break;
        case SimEngine::ComponentType::XNOR:
        case SimEngine::ComponentType::NOR: {
            info.inpPinStart = x + (w * 0.55) / 2.f;
            info.outPinStart = x1 + w * 0.25 + negCircleOff;
            info.inpConnStart = x - pinW;
            info.outConnStart = info.outPinStart + 20.f;
        } break;
        case SimEngine::ComponentType::NOT: {
            info.inpPinStart = x;
            info.outPinStart = x1 + negCircleOff;
            info.inpConnStart = info.inpPinStart - 20.f;
            info.outConnStart = info.outPinStart + 20.f;
        } break;
        default:
            w = scale.x, h = scale.y;
            x = pos.x - w / 2, x1 = pos.x + w / 2;
            y = pos.y - h / 2, y1 = pos.y + h / 2;
            info.inpPinStart = x;
            info.outPinStart = x1;
            info.inpConnStart = info.inpPinStart - 20.f;
            info.outConnStart = info.outPinStart + 20.f;
        }

        return info;
    }

    void Artist::setSchematicMode(bool value) {
        m_isSchematicMode = value;
    }

    bool Artist::getSchematicMode() {
        return m_isSchematicMode;
    }

    bool *Artist::getSchematicModePtr() {
        return &m_isSchematicMode;
    }
} // namespace Bess::Canvas
