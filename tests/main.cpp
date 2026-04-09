#include "gtest/gtest.h"
#include "pages/main_page/main_page.h"
#include "plugin_manager.h"
#include "simulation_engine.h"

namespace {
    class BessTestEnvironment : public testing::Environment {
      public:
        void SetUp() override {
            Bess::Pages::MainPage::setHeadless(true);
            Bess::Pages::MainPage::getInstance(nullptr);
        }

        void TearDown() override {
            Bess::Pages::MainPage::getInstance().reset();
            Bess::SimEngine::SimulationEngine::instance().destroy();
            Bess::Plugins::PluginManager::getInstance().destroy();
        }
    };
} // namespace

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    ::testing::AddGlobalTestEnvironment(new BessTestEnvironment());
    return RUN_ALL_TESTS();
}
