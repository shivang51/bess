#include "scene/scene_state/components/conn_joint_scene_component.h"
#include "scene/scene_state/components/connection_scene_component.h"
#include "scene/scene_state/scene_state.h"
#include "settings/viewport_theme.h"

namespace Bess::Canvas {
    ConnJointSceneComp::ConnJointSceneComp(UUID connectionId, int connSegIdx)
        : m_connSegIdx(connSegIdx), m_connectionId(connectionId) {
    }

    void ConnJointSceneComp::draw(SceneState &state,
                                  std::shared_ptr<Renderer::MaterialRenderer> materialRenderer,
                                  std::shared_ptr<Renderer2D::Vulkan::PathRenderer> pathRenderer) {
        const auto pickingId = PickingId{m_runtimeId, 0};
        materialRenderer->drawCircle(getAbsolutePosition(state),
                                     4.f,
                                     ViewportTheme::colors.selectedComp,
                                     pickingId);
    }

    void ConnJointSceneComp::drawSchematic(SceneState &state,
                                           std::shared_ptr<Renderer::MaterialRenderer> materialRenderer,
                                           std::shared_ptr<Renderer2D::Vulkan::PathRenderer> pathRenderer) {
        const auto pickingId = PickingId{m_runtimeId, 0};
        materialRenderer->drawCircle(getAbsolutePosition(state),
                                     4.f,
                                     ViewportTheme::colors.selectedComp,
                                     pickingId);
    }

    void ConnJointSceneComp::onMouseEnter(const Events::MouseEnterEvent &e) {}

    void ConnJointSceneComp::onMouseLeave(const Events::MouseLeaveEvent &e) {}

    glm::vec3 ConnJointSceneComp::getAbsolutePosition(const SceneState &state) const {
        const auto &conn = state.getComponentByUuid<ConnectionSceneComponent>(m_connectionId);

        const glm::vec3 &segStartPos = conn->getSegVertexPos(state, m_connSegIdx);
        const glm::vec3 &segEndPos = conn->getSegVertexPos(state, m_connSegIdx + 1);

        auto pos = glm::mix(segStartPos, segEndPos, m_segOffset);
        pos.z += 0.0001f;

        return pos;
    }
} // namespace Bess::Canvas
