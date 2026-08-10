#include "gtest/gtest.h"

namespace {
    class BessTestEnvironment : public testing::Environment {
      public:
        void SetUp() override {
        }

        void TearDown() override {
        }
    };
} // namespace

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    ::testing::AddGlobalTestEnvironment(new BessTestEnvironment());
    return RUN_ALL_TESTS();
}
