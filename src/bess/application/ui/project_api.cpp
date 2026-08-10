#include "ui/project_api.h"

#include "bess_core/g_app_context.h"
#include "pages/main_page/main_page.h"
#include "project_session/project_session.h"

#include <stdexcept>
#include <utility>

namespace Bess::UI::Proj {
    namespace {
        ProjectSession &sess() {
            const auto session =
                GAppContext::getInstance().getSubSystem<ProjectSession>();
            if (!session) {
                throw std::logic_error("project session is unavailable");
            }
            return *session;
        }

        Res result(const Status &status) {
            return {.ok = status.isOk(), .msg = status.msg()};
        }
    } // namespace

    SceneDriver &scenes() {
        return sess().scenes();
    }

    SimEngine::SimulationEngine &sim() {
        return sess().sim();
    }

    Res newProj() {
        return result(sess().newProj());
    }

    Res addConn(std::shared_ptr<Canvas::SceneComponent> conn, UUID scene) {
        const auto res = sess().addConn(std::move(conn), scene);
        return result(res.status);
    }

    LayoutRes layout() {
        const auto res = Pages::MainPage::getInstance()
                             ->getState()
                             .applyHierarchicalLayoutToActiveScene();
        return {.applied = res.applied, .count = res.laidOutNodes};
    }
} // namespace Bess::UI::Proj
