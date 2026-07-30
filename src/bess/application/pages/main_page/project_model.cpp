#include "pages/main_page/project_model.h"

#include "bess_core/scene/scene_ser_reg.h"
#include "pages/main_page/main_page_edit_hooks.h"
#include "pages/main_page/scene_components/conn_joint_scene_component.h"
#include "pages/main_page/scene_components/connection_scene_component.h"
#include "pages/main_page/scene_components/group_scene_component.h"
#include "pages/main_page/scene_components/image_scene_component.h"
#include "pages/main_page/scene_components/input_scene_component.h"
#include "pages/main_page/scene_components/module_scene_component.h"
#include "pages/main_page/scene_components/monitor_scene_comp.h"
#include "pages/main_page/scene_components/non_sim_scene_component.h"
#include "pages/main_page/scene_components/sim_scene_component.h"
#include "pages/main_page/scene_components/slot_probe_scene_component.h"
#include "pages/main_page/scene_components/slot_scene_component.h"
#include "pages/main_page/scene_components/text_scene_component.h"
#include "project_session/project_session.h"

namespace Bess::Pages {
    void initProjectModel(ProjectSession &session) {
        Canvas::NonSimSceneComponent::registerComponent<Canvas::TextComponent>(
            "Text Component");
        Canvas::NonSimSceneComponent::registerComponent<
            Canvas::ImageSceneComponent>("Image Component");
        Canvas::NonSimSceneComponent::registerComponent<
            Canvas::WidgetsTestComponent>("Widgets Test");
        Canvas::NonSimSceneComponent::registerComponent<
            Canvas::SlotProbeSceneComponent>("Probe");
        Canvas::NonSimSceneComponent::registerComponent<
            Canvas::MonitorSceneComp>("Monitor Node");

        REG_TO_SER_REGISTRY(Canvas::ConnJointSceneComp);
        REG_TO_SER_REGISTRY(Canvas::ConnectionSceneComponent);
        REG_TO_SER_REGISTRY(Canvas::GroupSceneComponent);
        REG_TO_SER_REGISTRY(Canvas::InputSceneComponent);
        REG_TO_SER_REGISTRY(Canvas::ImageSceneComponent);
        REG_TO_SER_REGISTRY(Canvas::NonSimSceneComponent);
        REG_TO_SER_REGISTRY(Canvas::SimulationSceneComponent);
        REG_TO_SER_REGISTRY(Canvas::SlotSceneComponent);
        REG_TO_SER_REGISTRY(Canvas::TextComponent);
        REG_TO_SER_REGISTRY(Canvas::WidgetsTestComponent);
        REG_TO_SER_REGISTRY(Canvas::SlotProbeSceneComponent);
        REG_TO_SER_REGISTRY(Canvas::ModuleSceneComponent);

        session.setHooks(makeEditHooks());
    }
} // namespace Bess::Pages
