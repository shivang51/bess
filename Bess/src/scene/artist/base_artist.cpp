#include "scene/artist/base_artist.h"
#include "asset_manager/asset_manager.h"
#include "assets.h"
#include "common/log.h"
#include "entt/entity/fwd.hpp"
#include "ext/vector_float3.hpp"
#include "scene/artist/artist_manager.h"
#include "scene/artist/nodes_artist.h"
#include "scene/components/components.h"
#include "scene/renderer/renderer.h"
#include "scene/scene.h"
#include "settings/viewport_theme.h"
#include "simulation_engine.h"
#include <cstdint>
#include <vector>

using Renderer = Bess::Renderer2D::Renderer;

namespace Bess::Canvas {
    ArtistTools BaseArtist::m_artistTools;

    BaseArtist::BaseArtist(Scene *scene) : m_sceneRef(scene) {
        static bool initialized = false;
        if (!initialized) {
            init();
            initialized = true;
        }
    }

    void BaseArtist::init() {
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

    void BaseArtist::drawGhostConnection(const entt::entity &startEntity, const glm::vec2 pos) {
        auto &registry = m_sceneRef->getEnttRegistry();
        auto slotsView = registry.view<Components::SlotComponent, Components::TransformComponent, Components::SimulationComponent>();
        auto &slotComp = slotsView.get<Components::SlotComponent>(startEntity);
        auto parentEntt = m_sceneRef->getEntityWithUuid(slotComp.parentId);
        auto &parentTransform = slotsView.get<Components::TransformComponent>(parentEntt);
        auto startPos = getSlotPos(slotComp, parentTransform);
        startPos.z = 0.f;

        float ratio = slotComp.slotType == Components::SlotType::digitalInput ? 0.8f : 0.2f;
        auto midX = startPos.x + ((pos.x - startPos.x) * ratio);

        Renderer::beginPathMode(startPos, 2.f, ViewportTheme::colors.ghostWire, -1);
        Renderer::pathLineTo(glm::vec3(midX, startPos.y, 0.f), 2.f, ViewportTheme::colors.ghostWire, -1);
        Renderer::pathLineTo(glm::vec3(midX, pos.y, 0.f), 2.f, ViewportTheme::colors.ghostWire, -1);
        Renderer::pathLineTo(glm::vec3(pos, 0.f), 2.f, ViewportTheme::colors.ghostWire, -1);
        Renderer::endPathMode();
    }

    void BaseArtist::drawConnection(const UUID &id, entt::entity inputEntity, entt::entity outputEntity, bool isSelected) {
        auto &registry = m_sceneRef->getEnttRegistry();
        auto connEntity = m_sceneRef->getEntityWithUuid(id);
        auto &connectionComponent = registry.get<Components::ConnectionComponent>(connEntity);

        const auto &inpSlotComp = registry.get<Components::SlotComponent>(inputEntity);
        const auto &outputSlotComp = registry.get<Components::SlotComponent>(outputEntity);
        const auto outParentEntt = m_sceneRef->getEntityWithUuid(outputSlotComp.parentId);
        const auto &outParentSimComp = registry.get<Components::SimulationComponent>(outParentEntt);

        glm::vec3 startPos, endPos;
        const auto &parentTransform = registry.get<Components::TransformComponent>(m_sceneRef->getEntityWithUuid(inpSlotComp.parentId));
        startPos = getSlotPos(inpSlotComp, parentTransform);
        const auto &endParentTransform = registry.get<Components::TransformComponent>(m_sceneRef->getEntityWithUuid(outputSlotComp.parentId));
        endPos = getSlotPos(outputSlotComp, endParentTransform);

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

        auto connSegEntt = m_sceneRef->getEntityWithUuid(connectionComponent.segmentHead);
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
                auto newSegEntt = m_sceneRef->getEntityWithUuid(newSegId);
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

            auto segEntt = m_sceneRef->getEntityWithUuid(segId);
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

    void BaseArtist::drawConnectionEntity(entt::entity entity) {
        auto &registry = m_sceneRef->getEnttRegistry();

        auto &connComp = registry.get<Components::ConnectionComponent>(entity);
        auto &idComp = registry.get<Components::IdComponent>(entity);
        bool isSelected = registry.all_of<Components::SelectedComponent>(entity);
        drawConnection(idComp.uuid, m_sceneRef->getEntityWithUuid(connComp.inputSlot), m_sceneRef->getEntityWithUuid(connComp.outputSlot), isSelected);
    }

    void BaseArtist::drawNonSimEntity(entt::entity entity) {
        auto &registry = m_sceneRef->getEnttRegistry();
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
            BESS_ERROR("[BaseArtist] Tried to draw a non-simulation component with type: {}", tagComp.type.typeId);
            break;
        }
    }

    void BaseArtist::drawSevenSegDisplay(
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

        auto &registry = m_sceneRef->getEnttRegistry();
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

        std::shared_ptr<ArtistManager> artists = m_sceneRef->getArtistManager();
        artists->getNodesArtist()->drawSlots(entity, simComp, transform);
    }

    void BaseArtist::setInstructions(const ArtistInstructions &value) {
        m_instructions = value;
    }

} // namespace Bess::Canvas
