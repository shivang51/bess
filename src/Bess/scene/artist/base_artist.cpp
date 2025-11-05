#include "scene/artist/base_artist.h"
#include "asset_manager/asset_manager.h"
#include "application/assets.h"
#include "common/log.h"
#include "plugin_manager.h"
#include "scene/artist/nodes_artist.h"
#include "scene/scene.h"
#include "scene/scene_pch.h"
#include "scene/viewport.h"
#include "settings/viewport_theme.h"
#include "simulation_engine.h"
#include "vulkan_core.h"
#include "vulkan_subtexture.h"
#include <vector>

using namespace Bess::Renderer2D;
namespace Bess::Canvas {
    ArtistTools BaseArtist::m_artistTools;

    BaseArtist::BaseArtist(const std::shared_ptr<Vulkan::VulkanDevice> &device,
                           const std::shared_ptr<Vulkan::VulkanOffscreenRenderPass> &renderPass,
                           VkExtent2D extent) {
        static bool initialized = false;
        if (!initialized) {
            init();
            initialized = true;
        }

        m_pathRenderer = std::make_shared<Renderer2D::Vulkan::PathRenderer>(device, renderPass, extent);
        m_materialRenderer = std::make_shared<Renderer::MaterialRenderer>(device, renderPass, extent);

        BESS_INFO("[Base Aritist] Initialized");
    }

    void BaseArtist::init() {
        auto tex = Assets::AssetManager::instance().get(Assets::TileMaps::sevenSegDisplay);
        float margin = 4.f;
        glm::vec2 size(128.f, 234.f);
        m_artistTools.sevenSegDispTexs = std::array<std::shared_ptr<Vulkan::SubTexture>, 8>{
            std::make_shared<Vulkan::SubTexture>(tex, glm::vec2({0.f, 0.f}), size, margin, glm::vec2(1.f)),
            std::make_shared<Vulkan::SubTexture>(tex, glm::vec2({1.f, 0.f}), size, margin, glm::vec2(1.f)),
            std::make_shared<Vulkan::SubTexture>(tex, glm::vec2({2.f, 0.f}), size, margin, glm::vec2(1.f)),
            std::make_shared<Vulkan::SubTexture>(tex, glm::vec2({3.f, 0.f}), size, margin, glm::vec2(1.f)),
            std::make_shared<Vulkan::SubTexture>(tex, glm::vec2({4.f, 0.f}), size, margin, glm::vec2(1.f)),
            std::make_shared<Vulkan::SubTexture>(tex, glm::vec2({0.f, 1.f}), size, margin, glm::vec2(1.f)),
            std::make_shared<Vulkan::SubTexture>(tex, glm::vec2({1.f, 1.f}), size, margin, glm::vec2(1.f)),
            std::make_shared<Vulkan::SubTexture>(tex, glm::vec2({2.f, 1.f}), size, margin, glm::vec2(1.f)),
        };

        Plugins::PluginManager &pluginMgr = Plugins::PluginManager::getInstance();
        const auto &loadedPlugins = pluginMgr.getLoadedPlugins();

        for (const auto &[name, plugin] : loadedPlugins) {
            auto schematicSymbols = plugin->onSchematicSymbolsLoad();
            m_artistTools.schematicSymbolPaths.insert(schematicSymbols.begin(), schematicSymbols.end());
            BESS_TRACE("[Base Artist] Loaded {} schematic symbols from plugin: {}", schematicSymbols.size(), name);
        }

        BESS_INFO("[Base Artist] Artist tools initialized");
    }

    void BaseArtist::begin(VkCommandBuffer cmd, const std::shared_ptr<Camera> &camera, uint32_t frameIdx) {
        m_pathRenderer->setCurrentFrameIndex(frameIdx);
        m_materialRenderer->setCurrentFrameIndex(frameIdx);

        m_pathRenderer->beginFrame(cmd);
        m_materialRenderer->beginFrame(cmd);

        Vulkan::UniformBufferObject ubo{};
        ubo.mvp = camera->getTransform();
        ubo.ortho = camera->getOrtho();
        m_pathRenderer->updateUniformBuffer(ubo);
        m_materialRenderer->updateUBO(camera);
    }

    void BaseArtist::end() {
        m_pathRenderer->endFrame();
        m_materialRenderer->endFrame();
    }

    void BaseArtist::drawGhostConnection(const entt::entity &startEntity, const glm::vec2 pos) {
        auto &registry = Scene::instance()->getEnttRegistry();
        const auto slotsView = registry.view<Components::SlotComponent, Components::TransformComponent, Components::SimulationComponent>();
        const auto &slotComp = slotsView.get<Components::SlotComponent>(startEntity);
        const auto parentEntt = Scene::instance()->getEntityWithUuid(slotComp.parentId);
        const auto &parentTransform = slotsView.get<Components::TransformComponent>(parentEntt);
        auto startPos = getSlotPos(slotComp, parentTransform);
        startPos.z = 0.8f;

        const float ratio = slotComp.slotType == Components::SlotType::digitalInput ? 0.75f : 0.25f;
        const auto midX = startPos.x + ((pos.x - startPos.x) * ratio);

        m_pathRenderer->beginPathMode(startPos, 2.f, ViewportTheme::colors.ghostWire, -1);
        m_pathRenderer->pathLineTo(glm::vec3(midX, startPos.y, 0.8f), 2.f, ViewportTheme::colors.ghostWire, -1);
        m_pathRenderer->pathLineTo(glm::vec3(midX, pos.y, 0.8f), 2.f, ViewportTheme::colors.ghostWire, -1);
        m_pathRenderer->pathLineTo(glm::vec3(pos, 0.8f), 2.f, ViewportTheme::colors.ghostWire, -1);
        m_pathRenderer->endPathMode(false, false, glm::vec4(1.f), true, true);
    }

    void BaseArtist::drawConnection(const UUID &id, entt::entity inputEntity, entt::entity outputEntity, bool isSelected) {
        auto &registry = Scene::instance()->getEnttRegistry();
        auto connEntity = Scene::instance()->getEntityWithUuid(id);
        auto &connectionComponent = registry.get<Components::ConnectionComponent>(connEntity);

        const auto &inpSlotComp = registry.get<Components::SlotComponent>(inputEntity);
        const auto &outputSlotComp = registry.get<Components::SlotComponent>(outputEntity);
        const auto outParentEntt = Scene::instance()->getEntityWithUuid(outputSlotComp.parentId);
        const auto &outParentSimComp = registry.get<Components::SimulationComponent>(outParentEntt);

        glm::vec3 startPos, endPos;
        const auto &parentTransform = registry.get<Components::TransformComponent>(Scene::instance()->getEntityWithUuid(inpSlotComp.parentId));
        startPos = getSlotPos(inpSlotComp, parentTransform);
        const auto &endParentTransform = registry.get<Components::TransformComponent>(Scene::instance()->getEntityWithUuid(outputSlotComp.parentId));
        endPos = getSlotPos(outputSlotComp, endParentTransform);

        startPos.z = 0.5f;
        endPos.z = 0.5f;

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

        auto connSegEntt = Scene::instance()->getEntityWithUuid(connectionComponent.segmentHead);
        auto connSegComp = registry.get<Components::ConnectionSegmentComponent>(connSegEntt);
        auto segId = connectionComponent.segmentHead;
        auto prevPos = startPos;

        static constexpr float wireSize = 2.f;
        static constexpr float hoveredSize = 3.f;

        bool isHovered = registry.all_of<Components::HoveredEntityComponent>(connSegEntt);

        m_pathRenderer->beginPathMode(startPos, isHovered ? hoveredSize : wireSize, color, static_cast<uint64_t>(connSegEntt));
        while (segId != UUID::null) {
            glm::vec3 pos = endPos;
            auto newSegId = connSegComp.next;

            if (newSegId != UUID::null) {
                auto newSegEntt = Scene::instance()->getEntityWithUuid(newSegId);
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

            auto segEntt = Scene::instance()->getEntityWithUuid(segId);
            isHovered = registry.all_of<Components::HoveredEntityComponent>(segEntt);
            auto size = isHovered ? hoveredSize : wireSize;
            auto offPos = pos;
            m_pathRenderer->pathLineTo(offPos, size, color, static_cast<uint64_t>(segEntt));
            offPos.z += 0.0001f;

            segId = newSegId;
            prevPos = pos;
        }
        m_pathRenderer->endPathMode(false, false, glm::vec4(1.f), true, true);
    }

    void BaseArtist::drawConnectionEntity(const entt::entity entity) {
        auto &registry = Scene::instance()->getEnttRegistry();

        const auto &connComp = registry.get<Components::ConnectionComponent>(entity);
        const auto &idComp = registry.get<Components::IdComponent>(entity);
        const bool isSelected = registry.all_of<Components::SelectedComponent>(entity);
        drawConnection(idComp.uuid, Scene::instance()->getEntityWithUuid(connComp.inputSlot), Scene::instance()->getEntityWithUuid(connComp.outputSlot), isSelected);
    }

    void BaseArtist::drawNonSimEntity(entt::entity entity) {
        auto &registry = Scene::instance()->getEnttRegistry();
        const auto &tagComp = registry.get<Components::TagComponent>(entity);
        const bool isSelected = registry.any_of<Components::SelectedComponent>(entity);

        const uint64_t id = static_cast<uint64_t>(entity);

        switch (tagComp.type.nsCompType) {
        case Components::NSComponentType::text: {
            const auto &textComp = registry.get<Components::TextNodeComponent>(entity);
            const auto &transformComp = registry.get<Components::TransformComponent>(entity);
            auto pos = transformComp.position;
            const auto rotation = transformComp.angle;
            m_materialRenderer->drawText(textComp.text, pos, textComp.fontSize, textComp.color, id, rotation);

            if (isSelected) {
                const Renderer::QuadRenderProperties props{
                    .angle = rotation,
                    .borderColor = ViewportTheme::colors.selectedComp,
                    .borderRadius = glm::vec4(4.f),
                    .borderSize = glm::vec4(1.f),
                    .isMica = true,
                };
                auto size = m_materialRenderer->getTextRenderSize(textComp.text, textComp.fontSize);
                pos.x += size.x * 0.5f;
                pos.y -= size.y * 0.25f;
                pos.z -= 0.0005f;
                size.x += componentStyles.paddingX * 2.f;
                size.y += componentStyles.paddingY * 2.f;
                m_materialRenderer->drawQuad(pos, size, ViewportTheme::colors.componentBG, id, props);
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

        const auto labelSize = m_materialRenderer->getTextRenderSize(name, componentStyles.headerFontSize);

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
        auto &registry = Scene::instance()->getEnttRegistry();
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

    void BaseArtist::destroyTools() {
        m_artistTools = {};
    }

    void BaseArtist::resize(VkExtent2D size) {
        m_pathRenderer->resize(size);
        m_materialRenderer->resize(size);
    }

    std::shared_ptr<Renderer2D::Vulkan::PathRenderer> BaseArtist::getPathRenderer() {
        return m_pathRenderer;
    }
    std::shared_ptr<Renderer::MaterialRenderer> BaseArtist::getMaterialRenderer() {
        return m_materialRenderer;
    }
} // namespace Bess::Canvas
