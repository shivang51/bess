#include "scene_new/artist.h"
#include "scene/renderer/renderer.h"
#include "scene_new/components/components.h"
#include "settings/viewport_theme.h"
#include <cstdint>

using Renderer = Bess::Renderer2D::Renderer;

namespace Bess::Canvas {
    Scene *Artist::sceneRef = nullptr;

    void Artist::drawSimEntity(entt::entity entity) {
        auto &registry = sceneRef->getEnttRegistry();

        auto &transformComp = registry.get<Components::TransformComponent>(entity);
        auto &spriteComp = registry.get<Components::SpriteComponent>(entity);

        auto [pos, rotation, scale] = transformComp.decompose();

        float headerHeight = 24.f;
        float radius = headerHeight / 2.f;
        auto headerPos = glm::vec3(pos.x, pos.y - scale.y / 2.f + headerHeight / 2.f, pos.z);

        spriteComp.borderRadius = glm::vec4(radius);

        Renderer::quad(pos, glm::vec2(scale), spriteComp.color, (uint64_t)entity, 0.f, spriteComp.borderRadius, spriteComp.borderSize, spriteComp.borderColor, true);

        Renderer::quad(headerPos,
                       glm::vec2(scale.x, headerHeight), ViewportTheme::compHeaderColor,
                       (uint64_t)entity,
                       0.f,
                       glm::vec4(0.f, 0.f, radius, radius),
                       spriteComp.borderSize, glm::vec4(0.f), true);
    }
} // namespace Bess::Canvas
