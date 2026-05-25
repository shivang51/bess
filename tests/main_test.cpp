#include "gtest/gtest.h"
#include "bess_core/g_app_context.h"
#include "pages/main_page/main_page.h"
#include "bess_core/project_context.h"
#include "plugin_manager.h"
#include "simulation_engine.h"

namespace {
    class BessTestEnvironment : public testing::Environment {
      public:
        void SetUp() override {
            auto &appCtx = Bess::GAppContext::getInstance();
            auto projectCtx = appCtx.getSubSystem<Bess::ProjectContext>();
            if (!projectCtx) {
                projectCtx = appCtx.addSubSystem<Bess::ProjectContext>();
            }
            if (!projectCtx->getSubSystem<Bess::SimEngine::SimulationEngine>()) {
                projectCtx->addSubSystem<Bess::SimEngine::SimulationEngine>();
            }

            Bess::Pages::MainPage::setHeadless(true);
            Bess::Pages::MainPage::getInstance(nullptr);
        }

        void TearDown() override {
            Bess::Pages::MainPage::getInstance().reset();
            Bess::GAppContext::getInstance().destroy();
            Bess::Plugins::PluginManager::getInstance().destroy();
        }
    };
} // namespace

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    ::testing::AddGlobalTestEnvironment(new BessTestEnvironment());
    return RUN_ALL_TESTS();
}
