#include "scene_new/artist.h"
#include "imgui.h"
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
        auto &tagComp = registry.get<Components::TagComponent>(entity);

        auto [pos, rotation, scale] = transformComp.decompose();
        pos.z = 0.f;

        float headerHeight = 24.f;
        float radius = headerHeight / 2.f;
        auto headerPos = glm::vec3(pos.x, pos.y - scale.y / 2.f + headerHeight / 2.f, pos.z);

        spriteComp.borderRadius = glm::vec4(radius);

        uint64_t id = (uint64_t)entity;

        glm::vec3 textPos = glm::vec3(pos.x - scale.x / 2.f + 12.f, headerPos.y, pos.z + 0.001f);

        Renderer::quad(pos, glm::vec2(scale), spriteComp.color, id, 0.f,
                       spriteComp.borderRadius,
                       spriteComp.borderSize, spriteComp.borderColor, true);

        Renderer::quad(headerPos,
                       glm::vec2(scale.x, headerHeight), ViewportTheme::compHeaderColor,
                       id,
                       0.f,
                       glm::vec4(0.f, 0.f, radius, radius),
                       spriteComp.borderSize, glm::vec4(0.f), true);

        Renderer::text(tagComp.name, textPos, 12.f, ViewportTheme::textColor, id);
    }
} // namespace Bess::Canvas
