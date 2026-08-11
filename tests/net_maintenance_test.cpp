#include "dig_sim_driver.h"
#include "math_sim_driver.h"

#include <gtest/gtest.h>
#include <memory>
#include <ranges>

namespace {
    using Bess::UUID;
    using namespace Bess::SimEngine;
    using namespace Bess::SimEngine::Drivers;
    using namespace Bess::SimEngine::Drivers::Digital;
    using namespace Bess::SimEngine::Drivers::Math;

    PortRef port(const UUID &componentId,
                 PortDirection direction,
                 SignalKind signalKind,
                 int index = 0) {
        return {.componentId = componentId,
                .direction = direction,
                .signalKind = signalKind,
                .index = index};
    }

    UUID addDigitalComponent(DigitalSimDriver &driver,
                             size_t portCount = 1,
                             bool resizeable = false) {
        auto definition = std::make_shared<DigCompDef>();
        definition->setName("Net test digital component");
        definition->setInputSlotsInfo(
            {SlotsGroupType::input, resizeable, portCount, {}, {}});
        definition->setOutputSlotsInfo(
            {SlotsGroupType::output, resizeable, portCount, {}, {}});
        definition->setSimFn(
            [](const std::shared_ptr<DigCompSimData> &data) { return data; });

        const auto component = driver.createComp(definition, false);
        EXPECT_NE(component, nullptr);
        return component ? driver.addComponent(component, false) : UUID::null;
    }

    UUID addMathComponent(MathSimDriver &driver,
                          size_t portCount = 1,
                          bool resizeable = false) {
        auto definition = std::make_shared<MathCompDef>();
        definition->setName("Net test math component");
        definition->setInputPortDescriptor({
            .direction = PortDirection::input,
            .signalKind = SignalKind::scalar,
            .quantityKind = QuantityKind::dimensionless,
            .count = portCount,
            .isResizeable = resizeable,
        });
        definition->setOutputPortDescriptor({
            .direction = PortDirection::output,
            .signalKind = SignalKind::scalar,
            .quantityKind = QuantityKind::dimensionless,
            .count = portCount,
            .isResizeable = resizeable,
        });
        definition->setSimFn(
            [](const std::shared_ptr<MathCompSimData> &data) { return data; });

        const auto component = driver.createComp(definition, false);
        EXPECT_NE(component, nullptr);
        return component ? driver.addComponent(component, false) : UUID::null;
    }

    template <typename TComponent, typename TDriver>
    void expectConsistentNets(const TDriver &driver) {
        const auto &components = driver.getComponentsMap();
        const auto &nets = driver.getNetsMap();
        size_t netComponentCount = 0;

        for (const auto &[netId, net] : nets) {
            EXPECT_EQ(net.getUUID(), netId);
            netComponentCount += net.size();
            for (const auto &componentId : net.getComponents()) {
                const auto component =
                    driver.template getComponent<TComponent>(componentId);
                ASSERT_NE(component, nullptr);
                EXPECT_EQ(component->getNetUuid(), netId);
            }
        }

        EXPECT_EQ(netComponentCount, components.size());
        for (const auto &[componentId, _] : components) {
            const auto component =
                driver.template getComponent<TComponent>(componentId);
            ASSERT_NE(component, nullptr);
            const auto netIt = nets.find(component->getNetUuid());
            ASSERT_NE(netIt, nets.end());
            EXPECT_EQ(
                std::ranges::count(netIt->second.getComponents(), componentId),
                1);
        }
    }

    TEST(NetMaintenanceTest, DigitalDriverSplitsNetWhenConnectionIsDeleted) {
        DigitalSimDriver driver;
        const auto first = addDigitalComponent(driver);
        const auto middle = addDigitalComponent(driver);
        const auto last = addDigitalComponent(driver);
        ASSERT_NE(first, UUID::null);
        ASSERT_NE(middle, UUID::null);
        ASSERT_NE(last, UUID::null);

        const auto firstToMiddle =
            std::pair{port(first, PortDirection::output, SignalKind::digital),
                      port(middle, PortDirection::input, SignalKind::digital)};
        ASSERT_TRUE(driver.connectPorts(
            firstToMiddle.first, firstToMiddle.second, false));
        ASSERT_TRUE(driver.connectPorts(
            port(middle, PortDirection::output, SignalKind::digital),
            port(last, PortDirection::input, SignalKind::digital),
            false));
        ASSERT_EQ(driver.getNetsMap().size(), 1);
        const auto oldNetId =
            driver.getComponent<DigSimComp>(middle)->getNetUuid();

        driver.clearNetUpdated();
        driver.deleteConnection(firstToMiddle.first, firstToMiddle.second);

        EXPECT_TRUE(driver.isNetUpdated());
        ASSERT_EQ(driver.getNetsMap().size(), 2);
        EXPECT_NE(driver.getComponent<DigSimComp>(first)->getNetUuid(),
                  driver.getComponent<DigSimComp>(middle)->getNetUuid());
        EXPECT_EQ(driver.getComponent<DigSimComp>(middle)->getNetUuid(),
                  oldNetId);
        EXPECT_EQ(driver.getComponent<DigSimComp>(last)->getNetUuid(),
                  oldNetId);
        expectConsistentNets<DigSimComp>(driver);
    }

    TEST(NetMaintenanceTest, DigitalDriverSplitsNetWhenBridgeIsDeleted) {
        DigitalSimDriver driver;
        const auto first = addDigitalComponent(driver);
        const auto bridge = addDigitalComponent(driver);
        const auto last = addDigitalComponent(driver);
        ASSERT_TRUE(driver.connectPorts(
            port(first, PortDirection::output, SignalKind::digital),
            port(bridge, PortDirection::input, SignalKind::digital),
            false));
        ASSERT_TRUE(driver.connectPorts(
            port(bridge, PortDirection::output, SignalKind::digital),
            port(last, PortDirection::input, SignalKind::digital),
            false));

        driver.clearNetUpdated();
        driver.deleteComponent(bridge);

        EXPECT_TRUE(driver.isNetUpdated());
        ASSERT_EQ(driver.getNetsMap().size(), 2);
        EXPECT_NE(driver.getComponent<DigSimComp>(first)->getNetUuid(),
                  driver.getComponent<DigSimComp>(last)->getNetUuid());
        expectConsistentNets<DigSimComp>(driver);
    }

    TEST(NetMaintenanceTest, MathDriverSplitsNetWhenConnectionIsDeleted) {
        MathSimDriver driver;
        const auto first = addMathComponent(driver);
        const auto middle = addMathComponent(driver);
        const auto last = addMathComponent(driver);
        ASSERT_NE(first, UUID::null);
        ASSERT_NE(middle, UUID::null);
        ASSERT_NE(last, UUID::null);

        const auto firstToMiddle =
            std::pair{port(first, PortDirection::output, SignalKind::scalar),
                      port(middle, PortDirection::input, SignalKind::scalar)};
        ASSERT_TRUE(driver.connectPorts(
            firstToMiddle.first, firstToMiddle.second, false));
        ASSERT_TRUE(driver.connectPorts(
            port(middle, PortDirection::output, SignalKind::scalar),
            port(last, PortDirection::input, SignalKind::scalar),
            false));
        ASSERT_EQ(driver.getNetsMap().size(), 1);
        const auto oldNetId =
            driver.getComponent<MathSimComp>(middle)->getNetUuid();

        driver.clearNetUpdated();
        driver.deleteConnection(firstToMiddle.first, firstToMiddle.second);

        EXPECT_TRUE(driver.isNetUpdated());
        ASSERT_EQ(driver.getNetsMap().size(), 2);
        EXPECT_NE(driver.getComponent<MathSimComp>(first)->getNetUuid(),
                  driver.getComponent<MathSimComp>(middle)->getNetUuid());
        EXPECT_EQ(driver.getComponent<MathSimComp>(middle)->getNetUuid(),
                  oldNetId);
        EXPECT_EQ(driver.getComponent<MathSimComp>(last)->getNetUuid(),
                  oldNetId);
        expectConsistentNets<MathSimComp>(driver);
    }

    TEST(NetMaintenanceTest, MathDriverSplitsNetWhenBridgeIsDeleted) {
        MathSimDriver driver;
        const auto first = addMathComponent(driver);
        const auto bridge = addMathComponent(driver);
        const auto last = addMathComponent(driver);
        ASSERT_TRUE(driver.connectPorts(
            port(first, PortDirection::output, SignalKind::scalar),
            port(bridge, PortDirection::input, SignalKind::scalar),
            false));
        ASSERT_TRUE(driver.connectPorts(
            port(bridge, PortDirection::output, SignalKind::scalar),
            port(last, PortDirection::input, SignalKind::scalar),
            false));

        driver.clearNetUpdated();
        driver.deleteComponent(bridge);

        EXPECT_TRUE(driver.isNetUpdated());
        ASSERT_EQ(driver.getNetsMap().size(), 2);
        EXPECT_NE(driver.getComponent<MathSimComp>(first)->getNetUuid(),
                  driver.getComponent<MathSimComp>(last)->getNetUuid());
        expectConsistentNets<MathSimComp>(driver);
    }

    TEST(NetMaintenanceTest,
         DigitalDriverKeepsConnectionsAndNetsValidWhenPortsMove) {
        DigitalSimDriver driver;
        const auto source = addDigitalComponent(driver);
        const auto target = addDigitalComponent(driver, 2, true);
        ASSERT_TRUE(driver.connectPorts(
            port(source, PortDirection::output, SignalKind::digital),
            port(target, PortDirection::input, SignalKind::digital, 1),
            false));

        ASSERT_TRUE(
            driver
                .addPort(
                    port(target, PortDirection::input, SignalKind::digital, 0),
                    true)
                .hasChange());
        ASSERT_EQ(driver.getComponent<DigSimComp>(source)
                      ->getOutputConnections()[0][0]
                      .second,
                  2);

        ASSERT_TRUE(
            driver
                .removePort(
                    port(target, PortDirection::input, SignalKind::digital, 1),
                    true)
                .hasChange());
        ASSERT_EQ(driver.getComponent<DigSimComp>(source)
                      ->getOutputConnections()[0][0]
                      .second,
                  1);
        EXPECT_EQ(driver.getNetsMap().size(), 1);

        ASSERT_TRUE(
            driver
                .removePort(
                    port(target, PortDirection::input, SignalKind::digital, 1),
                    true)
                .hasChange());
        EXPECT_TRUE(driver.getComponent<DigSimComp>(source)
                        ->getOutputConnections()[0]
                        .empty());
        EXPECT_EQ(driver.getNetsMap().size(), 2);
        expectConsistentNets<DigSimComp>(driver);
    }

    TEST(NetMaintenanceTest,
         MathDriverKeepsConnectionsAndNetsValidWhenPortsMove) {
        MathSimDriver driver;
        const auto source = addMathComponent(driver);
        const auto target = addMathComponent(driver, 2, true);
        ASSERT_TRUE(driver.connectPorts(
            port(source, PortDirection::output, SignalKind::scalar),
            port(target, PortDirection::input, SignalKind::scalar, 1),
            false));

        ASSERT_TRUE(
            driver
                .addPort(
                    port(target, PortDirection::input, SignalKind::scalar, 0),
                    true)
                .hasChange());
        ASSERT_EQ(driver.getComponent<MathSimComp>(source)
                      ->getOutputConnections()[0][0]
                      .second,
                  2);

        ASSERT_TRUE(
            driver
                .removePort(
                    port(target, PortDirection::input, SignalKind::scalar, 1),
                    true)
                .hasChange());
        ASSERT_EQ(driver.getComponent<MathSimComp>(source)
                      ->getOutputConnections()[0][0]
                      .second,
                  1);
        EXPECT_EQ(driver.getNetsMap().size(), 1);

        ASSERT_TRUE(
            driver
                .removePort(
                    port(target, PortDirection::input, SignalKind::scalar, 1),
                    true)
                .hasChange());
        EXPECT_TRUE(driver.getComponent<MathSimComp>(source)
                        ->getOutputConnections()[0]
                        .empty());
        EXPECT_EQ(driver.getNetsMap().size(), 2);
        expectConsistentNets<MathSimComp>(driver);
    }
} // namespace
