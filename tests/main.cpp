#include "gtest/gtest.h"
#include "plugin_manager.h"

namespace {
    class BessTestEnvironment : public testing::Environment {
      public:
        void TearDown() override {
            Bess::Plugins::PluginManager::getInstance().destroy();
        }
    };
} // namespace

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    ::testing::AddGlobalTestEnvironment(new BessTestEnvironment());
    return RUN_ALL_TESTS();
}
