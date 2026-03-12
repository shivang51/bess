#include "non_sim_scene_component.h"
#include "scene/scene_state/components/styles/comp_style.h"
#include "scene_draw_context.h"
#include <unordered_map>

namespace Bess::Canvas {
    std::shared_ptr<NonSimSceneComponent> NonSimSceneComponent::getInstance(std::type_index tIdx) {
        auto &m_contrRegistry = getContrRegistry();
        if (m_contrRegistry.contains(tIdx)) {
            return m_contrRegistry[tIdx]();
        }
        return nullptr;
    }

    void TextComponent::draw(SceneDrawContext &context) {
        if (m_isFirstDraw) {
            onFirstDraw(context);
            m_isFirstDraw = false;
        }

        auto &state = *context.sceneState;
        if (m_isScaleDirty) {
            m_transform.scale = calculateScale(state);
            m_isScaleDirty = false;
        }

        const auto pickingId = PickingId{m_runtimeId, 0};
        context.materialRenderer->drawText(m_data,
                                           m_transform.position,
                                           m_size,
                                           m_foregroundColor,
                                           pickingId);

        // draw background if selected
        if (m_isSelected) {
            Renderer::QuadRenderProperties props;
            props.angle = m_transform.angle;
            props.borderRadius = m_style.borderRadius;
            props.borderSize = m_style.borderSize;
            props.borderColor = ViewportTheme::colors.selectedComp;
            props.isMica = true;
            props.shadow.enabled = true;

            const auto textSize = Renderer::MaterialRenderer::getTextRenderSize(m_data, (float)m_size);

            auto offset = glm::vec3((m_transform.scale.x / 2.f) - Styles::componentStyles.paddingX,
                                    (-textSize.y / 4.f) - Styles::componentStyles.paddingY,
                                    -0.0001f);

            context.materialRenderer->drawQuad(m_transform.position + offset,
                                               m_transform.scale,
                                               m_style.color,
                                               pickingId,
                                               props);
        }
    }

    glm::vec2 TextComponent::calculateScale(SceneState &state) {
        auto textSize = Renderer::MaterialRenderer::getTextRenderSize(m_data, (float)m_size);
        textSize.y += Styles::componentStyles.paddingY * 2.f;
        textSize.x += Styles::componentStyles.paddingX * 2.f;
        return textSize;
    }
    std::type_index NonSimSceneComponent::getTypeIndex() {
        return {typeid(void)};
    }
    void NonSimSceneComponent::clearRegistry() {
        getRegistry().clear();
        getContrRegistry().clear();
    }

    std::unordered_map<std::type_index, std::string> &NonSimSceneComponent::getRegistry() {
        static std::unordered_map<std::type_index, std::string> registry;
        return registry;
    }

    std::unordered_map<std::type_index, NonSimSceneComponent::ContrFunc> &
    NonSimSceneComponent::getContrRegistry() {
        static std::unordered_map<std::type_index, ContrFunc> reg;
        return reg;
    }
} // namespace Bess::Canvas
