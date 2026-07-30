#include "pages/main_page/project_model.h"

#include "bess_core/scene/scene_ser_reg.h"
#include "bess_core/g_app_context.h"
#include "bess_core/scene_driver.h"
#include "common/bess_assert.h"
#include "common/logger.h"
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
#include "pages/main_page/services/connection_service.h"
#include "project_session/project_session.h"
#include "services/plugin_service/plugin_service.h"

namespace Bess::Pages {
    void initProjectModel(ProjectSession &session) {
        const auto conn = session.getSubSystem<Svc::SvcConnection>();
        BESS_ASSERT(conn, "Project connection service is unavailable");
        if (!conn) {
            BESS_ERROR("Could not initialize project model without the "
                       "connection service");
            return;
        }
        conn->setSimEngine(&session.sim());
        auto *scenes = &session.scenes();
        session.scenes().setConnDepsFn(
            [scenes, weak = std::weak_ptr<Svc::SvcConnection>(conn)](
                UUID sceneId, UUID connId) {
                const auto svc = weak.lock();
                const auto scene = scenes->getSceneWithId(sceneId);
                return svc && scene ? svc->getDependants(connId, scene)
                                    : std::vector<UUID>{};
            });

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

        Canvas::SceneSerReg::setFallback(
            [](const Json::Value &json)
                -> std::shared_ptr<Canvas::SceneComponent> {
                const auto &app = GAppContext::getInstance();
                if (!app.hasSubSystem<Svc::PluginService>()) {
                    return nullptr;
                }
                const auto plugins = app.getSubSystem<Svc::PluginService>();
                const auto type = json["typeName"].asString();
                return plugins->canDerserialize(type)
                           ? plugins->derserialize(type, json)
                           : nullptr;
            });

        session.setHooks(makeEditHooks(conn));
    }
} // namespace Bess::Pages
