#include "services/window_drop_service/window_drop_service.h"

#include <gtest/gtest.h>

namespace {
    using namespace Bess;

    TEST(WindowDropServiceTests,
         UsesDistinctNonZeroTokensAndAcceptsIdempotentUnsubscribe) {
        Svc::WindowDropService service;

        EXPECT_EQ(service.subscribe({}), 0U);
        EXPECT_EQ(service.subscribeDrag({}), 0U);
        EXPECT_EQ(service.subscribeDragDecision({}), 0U);

        const auto committed =
            service.subscribe([](const Events::WindowDropEvent &) {});
        const auto lifecycle =
            service.subscribeDrag([](const Events::WindowDragDropEvent &) {});
        const auto decision = service.subscribeDragDecision(
            [](const Events::WindowDragDropEvent &) { return true; });
        ASSERT_NE(committed, 0U);
        ASSERT_NE(lifecycle, 0U);
        ASSERT_NE(decision, 0U);
        EXPECT_NE(committed, lifecycle);
        EXPECT_NE(committed, decision);
        EXPECT_NE(lifecycle, decision);

        service.unsubscribe(committed);
        service.unsubscribe(committed);
        service.unsubscribeDrag(lifecycle);
        service.unsubscribeDrag(lifecycle);
        service.unsubscribeDragDecision(decision);
        service.unsubscribeDragDecision(decision);
    }
} // namespace
