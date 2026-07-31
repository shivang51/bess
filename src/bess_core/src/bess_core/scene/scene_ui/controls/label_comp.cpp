#include "bess_core/scene/scene_ui/controls/label_comp.h"
#include "bess_core/scene/scene_state/scene_state.h"

namespace Bess::Canvas::UI {
    std::shared_ptr<LabelComp> LabelComp::create(const CompConfig &config) {
        return create("", config);
    }

    std::shared_ptr<LabelComp> LabelComp::create(const std::string &label,
                                                 const CompConfig &config) {
        auto labelComp = std::make_shared<LabelComp>();
        labelComp->setName(label);
        applyCompConfig(labelComp, config);
        return labelComp;
    }

    void LabelComp::onDraw(SceneDrawContext &state) {
        drawText(state, m_name, m_node, resolveLabelPickingId(state));
    }

    Core::Viewport::SceneCursor LabelComp::getCursor() const {
        return Core::Viewport::SceneCursor::normal;
    }

    PickingId LabelComp::resolveLabelPickingId(SceneDrawContext &state) const {
        if (m_parentComponent == UUID::null || state.sceneState == nullptr) {
            return PickingId::invalid();
        }

        auto *parent = state.sceneState->getComponentByUuid(m_parentComponent);
        if (parent == nullptr) {
            return PickingId::invalid();
        }

        uint32_t runtimeId = parent->getRuntimeId();
        if (parent->getType() == SceneComponentType::ui) {
            const auto *uiParent =
                static_cast<const UISceneComponent *>(parent);
            runtimeId = uiParent->getDrawRuntimeId().value_or(runtimeId);
        }

        if (runtimeId == PickingId::invalidRuntimeId) {
            return PickingId::invalid();
        }

        return PickingId{
            .runtimeId = runtimeId,
            .info = PickingId::InfoFlags::passiveCursor,
        };
    }
} // namespace Bess::Canvas::UI
