#include "scene/artist/base_artist.h"
#include "asset_manager/asset_manager.h"
#include "assets.h"
#include "common/log.h"
#include "entt/entity/fwd.hpp"
#include "ext/vector_float3.hpp"
#include "scene/artist/nodes_artist.h"
#include "scene/components/components.h"
#include "scene/renderer/vulkan/vulkan_core.h"
#include "scene/renderer/vulkan/vulkan_subtexture.h"
#include "scene/renderer/vulkan/vulkan_texture.h"
#include "scene/renderer/vulkan_renderer.h"
#include "scene/scene.h"
#include "settings/viewport_theme.h"
#include "simulation_engine.h"
#include <cstdint>
#include <vector>

using Renderer = Bess::Renderer2D::VulkanCore;
using namespace Bess::Renderer2D;
namespace Bess::Canvas {
    ArtistTools BaseArtist::m_artistTools;

    BaseArtist::BaseArtist(std::shared_ptr<Scene> scene) : m_sceneRef(scene) {
        static bool initialized = false;
        if (!initialized) {
            init();
            initialized = true;
        }
    }

    void BaseArtist::init() {
        auto tex = Assets::AssetManager::instance().get(Assets::TileMaps::sevenSegDisplay);
        float margin = 4.F;
        glm::vec2 size(128.F, 234.F);
        m_artistTools.sevenSegDispTexs = std::array<std::shared_ptr<Vulkan::SubTexture>, 8>{
            std::make_shared<Vulkan::SubTexture>(tex, glm::vec2({0.F, 0.F}), size),
            std::make_shared<Vulkan::SubTexture>(tex, glm::vec2({1.F, 0.F}), size),
            std::make_shared<Vulkan::SubTexture>(tex, glm::vec2({2.F, 0.F}), size),
            std::make_shared<Vulkan::SubTexture>(tex, glm::vec2({3.F, 0.F}), size),
            std::make_shared<Vulkan::SubTexture>(tex, glm::vec2({4.F, 0.F}), size),
            std::make_shared<Vulkan::SubTexture>(tex, glm::vec2({0.F, 1.F}), size),
            std::make_shared<Vulkan::SubTexture>(tex, glm::vec2({1.F, 1.F}), size),
            std::make_shared<Vulkan::SubTexture>(tex, glm::vec2({2.F, 1.F}), size),
        };
    }

    void BaseArtist::drawGhostConnection(const entt::entity &startEntity, const glm::vec2 pos) {
        auto &registry = m_sceneRef->getEnttRegistry();
        const auto slotsView = registry.view<Components::SlotComponent, Components::TransformComponent, Components::SimulationComponent>();
        const auto &slotComp = slotsView.get<Components::SlotComponent>(startEntity);
        const auto parentEntt = m_sceneRef->getEntityWithUuid(slotComp.parentId);
        const auto &parentTransform = slotsView.get<Components::TransformComponent>(parentEntt);
        auto startPos = getSlotPos(slotComp, parentTransform);
        startPos.z = 0.f;

        const float ratio = slotComp.slotType == Components::SlotType::digitalInput ? 0.8f : 0.2f;
        const auto midX = startPos.x + ((pos.x - startPos.x) * ratio);

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
            bool isHigh = static_cast<bool>(SimEngine::SimulationEngine::instance().getComponentState(outParentSimComp.simEngineEntity).outputStates[outputSlotComp.idx]);
            color = isHigh ? ViewportTheme::colors.stateHigh : ViewportTheme::colors.stateLow;
        }

        color = isSelected ? ViewportTheme::colors.selectedWire : color;

        auto connSegEntt = m_sceneRef->getEntityWithUuid(connectionComponent.segmentHead);
        auto connSegComp = registry.get<Components::ConnectionSegmentComponent>(connSegEntt);
        auto segId = connectionComponent.segmentHead;
        auto prevPos = startPos;

        static constexpr float wireSize = 2.f;
        static constexpr float hoveredSize = 3.f;

        bool isHovered = registry.all_of<Components::HoveredEntityComponent>(connSegEntt);

        Renderer::beginPathMode(startPos, isHovered ? hoveredSize : wireSize, color, static_cast<uint64_t>(connSegEntt));
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
            isHovered = registry.all_of<Components::HoveredEntityComponent>(segEntt);
            auto size = isHovered ? hoveredSize : wireSize;
            auto offPos = pos;
            Renderer::pathLineTo(offPos, size, color, static_cast<uint64_t>(segEntt));
            offPos.z += 0.0001f;

            segId = newSegId;
            prevPos = pos;
        }
        Renderer::endPathMode();
    }

    void BaseArtist::drawConnectionEntity(const entt::entity entity) {
        auto &registry = m_sceneRef->getEnttRegistry();

        const auto &connComp = registry.get<Components::ConnectionComponent>(entity);
        const auto &idComp = registry.get<Components::IdComponent>(entity);
        const bool isSelected = registry.all_of<Components::SelectedComponent>(entity);
        drawConnection(idComp.uuid, m_sceneRef->getEntityWithUuid(connComp.inputSlot), m_sceneRef->getEntityWithUuid(connComp.outputSlot), isSelected);
    }

    void BaseArtist::drawNonSimEntity(entt::entity entity) {
        auto &registry = m_sceneRef->getEnttRegistry();
        const auto &tagComp = registry.get<Components::TagComponent>(entity);
        const bool isSelected = registry.any_of<Components::SelectedComponent>(entity);

        const uint64_t id = static_cast<uint64_t>(entity);

        switch (tagComp.type.nsCompType) {
        case Components::NSComponentType::text: {
            const auto &textComp = registry.get<Components::TextNodeComponent>(entity);
            const auto &transformComp = registry.get<Components::TransformComponent>(entity);
            auto pos = transformComp.position;
            const auto rotation = transformComp.angle;
            Renderer::msdfText(textComp.text, pos, textComp.fontSize, textComp.color, id, rotation);

            if (isSelected) {
                const Renderer2D::QuadRenderProperties props{
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
                VulkanRenderer::quad(pos, size, ViewportTheme::colors.componentBG, id, props);
            }
        } break;
        default:
            BESS_ERROR("[BaseArtist] Tried to draw a non-simulation component with type: {}", tagComp.type.typeId);
            break;
        }
    }

    glm::vec2 BaseArtist::calcCompSize(entt::entity ent,
                                       const Components::SimulationComponent &simComp,
                                       const std::string &name) {

        const int maxRows = std::max(simComp.inputSlots.size(), simComp.outputSlots.size());
        float height = (maxRows * SLOT_ROW_SIZE);

        const auto labelSize = Renderer::getMSDFTextRenderSize(name, componentStyles.headerFontSize);

        float width = labelSize.x + componentStyles.paddingX * 2.f;

        if (!isHeaderLessComp(simComp)) {
            width = std::max(width, 100.f);
            height += componentStyles.headerHeight + componentStyles.rowGap;
        } else { // header less component

            if (!simComp.inputSlots.empty()) {
                constexpr float labelXGapL = 2.f;
                width += SLOT_COLUMN_SIZE + labelXGapL;
            }

            if (!simComp.outputSlots.empty()) {
                constexpr float labelXGapR = 2.f;
                width += SLOT_COLUMN_SIZE + labelXGapR;
            }
        }

        return {width, height};
    }

    void BaseArtist::setInstructions(const ArtistInstructions &value) {
        m_instructions = value;
    }

    void BaseArtist::drawSimEntity(const entt::entity entity) {
        auto &registry = m_sceneRef->getEnttRegistry();
        const auto &tagComp = registry.get<Components::TagComponent>(entity);
        const auto &transform = registry.get<Components::TransformComponent>(entity);
        const auto &spriteComp = registry.get<Components::SpriteComponent>(entity);
        const auto &simComp = registry.get<Components::SimulationComponent>(entity);

        drawSimEntity(entity, tagComp, transform, spriteComp, simComp);
    }

    bool BaseArtist::isHeaderLessComp(const Components::SimulationComponent &simComp) {
        const int n = std::max(simComp.inputSlots.size(), simComp.outputSlots.size());
        return n == 1;
    }
} // namespace Bess::Canvas
