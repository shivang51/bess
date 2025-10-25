#pragma once

#include "bess_uuid.h"
#include "camera.h"
#include "entt/entity/fwd.hpp"
#include "ext/vector_float3.hpp"
#include "scene/components/components.h"
#include "scene/renderer/material_renderer.h"
#include "scene/renderer/vulkan/path_renderer.h"
#include "types.h"
#include "vulkan_subtexture.h"

#include <memory>
#include <vulkan/vulkan_core.h>

namespace Bess::Canvas {
    class Viewport;
}
using namespace Bess::Renderer2D;

namespace Bess::Canvas {

    struct ArtistTools {
        std::array<std::shared_ptr<Vulkan::SubTexture>, 8> sevenSegDispTexs;
        std::unordered_map<uint64_t, Renderer::Path> schematicSymbolPaths;
    };

    struct ArtistInstructions {
        bool isSchematicView = false;
    };

    class BaseArtist {
      public:
        explicit BaseArtist(const std::shared_ptr<Vulkan::VulkanDevice> &device,
                            const std::shared_ptr<Vulkan::VulkanOffscreenRenderPass> &renderPass,
                            VkExtent2D extent);
        virtual ~BaseArtist() = default;

        static void destroyTools();
        static void init();

        void begin(VkCommandBuffer cmd, const std::shared_ptr<Camera> &camera, uint32_t frameIdx);
        void end();

        glm::vec2 calcCompSize(entt::entity ent,
                               const Components::SimulationComponent &simComp,
                               const std::string &name);

        static bool isHeaderLessComp(const Components::SimulationComponent &simComp);

        virtual void drawSimEntity(entt::entity entity);

        virtual void drawSimEntity(
            entt::entity entity,
            const Components::TagComponent &tagComp,
            const Components::TransformComponent &transform,
            const Components::SpriteComponent &spriteComp,
            const Components::SimulationComponent &simComponent) = 0;

        virtual void drawNonSimEntity(entt::entity entity);

        virtual void drawConnectionEntity(entt::entity entity);

        virtual void drawGhostConnection(const entt::entity &startEntity, const glm::vec2 pos);

        virtual glm::vec3 getSlotPos(const Components::SlotComponent &comp,
                                     const Components::TransformComponent &parentTransform) = 0;

        void setInstructions(const ArtistInstructions &value);

        void resize(VkExtent2D size);

        std::shared_ptr<Renderer2D::Vulkan::PathRenderer> getPathRenderer();
        std::shared_ptr<Renderer::MaterialRenderer> getMaterialRenderer();

      protected:
        virtual void drawSlots(const entt::entity parentEntt, const Components::SimulationComponent &comp, const Components::TransformComponent &transformComp) = 0;

        virtual void drawConnection(const UUID &id, entt::entity inputEntity, entt::entity outputEntity, bool isSelected);

        static ArtistTools m_artistTools;

        ArtistInstructions m_instructions = {};
        std::shared_ptr<Renderer2D::Vulkan::PathRenderer> m_pathRenderer;
        std::shared_ptr<Renderer::MaterialRenderer> m_materialRenderer;
    };

} // namespace Bess::Canvas
