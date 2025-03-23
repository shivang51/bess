#pragma once

#include "component_catalog.h"
#include "entt_components.h"
#include "types.h"

namespace Bess::SimEngine {

    void initComponentCatalog() {
        ComponentCatalog::instance().registerComponent(ComponentDefinition(ComponentType::INPUT, "Input", "IO", 0, 1, [](entt::registry &, entt::entity, auto fn) -> bool { return false; }, SimDelayMilliSeconds(100)));

        ComponentCatalog::instance().registerComponent({ComponentType::OUTPUT, "Output", "IO", 1, 0,
                                                        [&](entt::registry &registry, entt::entity e, auto fn) -> bool {
                                                            auto &gate = registry.get<GateComponent>(e);
                                                            bool newState = false;
                                                            if (!gate.inputPins.empty()) {
                                                                bool pinState = false;
                                                                for (const auto &conn : gate.inputPins[0]) {
                                                                    auto e = fn(conn.first);
                                                                    if (registry.valid(e)) {
                                                                        auto &srcGate = registry.get<GateComponent>(e);
                                                                        if (!srcGate.outputStates.empty()) {
                                                                            pinState = pinState || srcGate.outputStates[0];
                                                                        }
                                                                    }
                                                                }
                                                                newState = pinState;
                                                            }
                                                            bool changed = false;
                                                            for (auto state : gate.outputStates) {
                                                                if (state != newState) {
                                                                    state = newState;
                                                                    changed = true;
                                                                }
                                                            }
                                                            return changed;
                                                        },
                                                        SimDelayMilliSeconds(100)});

        /*ComponentDefinition andGate = {ComponentType::AND, "AND Gate", "Digital Gates", 2, 1,*/
        /*                               [](entt::registry &registry, entt::entity e) -> bool {*/
        /*                                   auto &gate = registry.get<GateComponent>(e);*/
        /*                                   std::vector<bool> pinValues;*/
        /*                                   for (const auto &pin : gate.inputPins) {*/
        /*                                       bool pinState = false;*/
        /*                                       for (const auto &conn : pin) {*/
        /*                                           if (registry.valid(conn.first)) {*/
        /*                                               auto &srcGate = registry.get<GateComponent>(conn.first);*/
        /*                                               if (!srcGate.outputStates.empty()) {*/
        /*                                                   pinState = pinState || srcGate.outputStates[conn.second];*/
        /*                                                   if (pinState)*/
        /*                                                       break;*/
        /*                                               }*/
        /*                                           }*/
        /*                                       }*/
        /*                                       pinValues.push_back(pinState);*/
        /*                                   }*/
        /*                                   bool newState = true;*/
        /*                                   for (auto state : pinValues) {*/
        /*                                       newState = newState && state;*/
        /*                                       if (!newState)*/
        /*                                           break;*/
        /*                                   }*/
        /**/
        /*                                   bool changed = false;*/
        /*                                   for (auto state : gate.outputStates) {*/
        /*                                       if (state != newState) {*/
        /*                                           state = newState;*/
        /*                                           changed = true;*/
        /*                                       }*/
        /*                                   }*/
        /*                                   return changed;*/
        /*                               },*/
        /*                               SimDelayMilliSeconds(100)};*/
        /**/
        /*andGate.addModifiableProperty(Properties::ComponentProperty::inputCount, {3, 4, 5});*/
        /**/
        /*ComponentCatalog::instance().registerComponent(andGate);*/
        /**/
        /*ComponentCatalog::instance().registerComponent({ComponentType::OR, "OR Gate", "Digital Gates", 2, 1,*/
        /*                                                [](entt::registry &registry, entt::entity e) -> bool {*/
        /*                                                    auto &gate = registry.get<GateComponent>(e);*/
        /*                                                    std::vector<bool> pinValues;*/
        /*                                                    for (const auto &pin : gate.inputPins) {*/
        /*                                                        bool pinState = false;*/
        /*                                                        for (const auto &conn : pin) {*/
        /*                                                            if (registry.valid(conn.first)) {*/
        /*                                                                auto &srcGate = registry.get<GateComponent>(conn.first);*/
        /*                                                                if (!srcGate.outputStates.empty()) {*/
        /*                                                                    pinState = pinState || srcGate.outputStates[0];*/
        /*                                                                }*/
        /*                                                            }*/
        /*                                                        }*/
        /*                                                        pinValues.push_back(pinState);*/
        /*                                                    }*/
        /*                                                    bool newState = (pinValues.size() >= 2) ? (pinValues[0] || pinValues[1]) : false;*/
        /*                                                    bool changed = false;*/
        /*                                                    for (auto state : gate.outputStates) {*/
        /*                                                        if (state != newState) {*/
        /*                                                            state = newState;*/
        /*                                                            changed = true;*/
        /*                                                        }*/
        /*                                                    }*/
        /*                                                    return changed;*/
        /*                                                },*/
        /*                                                SimDelayMilliSeconds(100)});*/
        /**/
        /*ComponentDefinition notGate = {ComponentType::NOT, "NOT Gate", "Digital Gates", 1, 1,*/
        /*                               [](entt::registry &registry, entt::entity e) -> bool {*/
        /*                                   auto &gate = registry.get<GateComponent>(e);*/
        /*                                   std::vector<bool> newStates = {};*/
        /*                                   if (!gate.inputPins.empty()) {*/
        /*                                       for (const auto &pin : gate.inputPins) {*/
        /*                                           bool pinState = false;*/
        /*                                           for (const auto &conn : pin) {*/
        /*                                               if (registry.valid(conn.first)) {*/
        /*                                                   auto &srcGate = registry.get<GateComponent>(conn.first);*/
        /*                                                   if (!srcGate.outputStates.empty()) {*/
        /*                                                       pinState = pinState || srcGate.outputStates[0];*/
        /*                                                   }*/
        /*                                               }*/
        /*                                           }*/
        /*                                           newStates.emplace_back(!pinState);*/
        /*                                       }*/
        /*                                   }*/
        /*                                   bool changed = false;*/
        /*                                   for (int i = 0; i < newStates.size(); i++) {*/
        /*                                       if (gate.outputStates[i] != newStates[i]) {*/
        /*                                           changed = true;*/
        /*                                           break;*/
        /*                                       }*/
        /*                                   }*/
        /*                                   if (changed)*/
        /*                                       gate.outputStates = newStates;*/
        /*                                   return changed;*/
        /*                               },*/
        /*                               SimDelayMilliSeconds(100)};*/
        /*notGate.addModifiableProperty(Properties::ComponentProperty::inputCount, {2, 3, 4, 5, 6});*/
        /*ComponentCatalog::instance().registerComponent(notGate);*/
        /**/
        /*ComponentCatalog::instance().registerComponent({ComponentType::XOR, "XOR Gate", "Digital Gates", 2, 1,*/
        /*                                                [](entt::registry &registry, entt::entity e) -> bool {*/
        /*                                                    auto &gate = registry.get<GateComponent>(e);*/
        /*                                                    std::vector<bool> pinValues;*/
        /*                                                    for (const auto &pin : gate.inputPins) {*/
        /*                                                        bool pinState = false;*/
        /*                                                        for (const auto &conn : pin) {*/
        /*                                                            if (registry.valid(conn.first)) {*/
        /*                                                                auto &srcGate = registry.get<GateComponent>(conn.first);*/
        /*                                                                if (!srcGate.outputStates.empty()) {*/
        /*                                                                    pinState = pinState || srcGate.outputStates[0];*/
        /*                                                                }*/
        /*                                                            }*/
        /*                                                        }*/
        /*                                                        pinValues.push_back(pinState);*/
        /*                                                    }*/
        /*                                                    bool newState = (pinValues.size() >= 2) ? (pinValues[0] ^ pinValues[1]) : false;*/
        /*                                                    bool changed = false;*/
        /*                                                    for (auto state : gate.outputStates) {*/
        /*                                                        if (state != newState) {*/
        /*                                                            state = newState;*/
        /*                                                            changed = true;*/
        /*                                                        }*/
        /*                                                    }*/
        /*                                                    return changed;*/
        /*                                                },*/
        /*                                                SimDelayMilliSeconds(100)});*/
        /**/
        /*ComponentCatalog::instance().registerComponent(ComponentDefinition(ComponentType::XNOR, "XNOR Gate", "Digital Gates", 2, 1, [](entt::registry &registry, entt::entity e) -> bool {*/
        /*                                                    auto &gate = registry.get<GateComponent>(e);*/
        /*                                                    std::vector<bool> pinValues;*/
        /*                                                    for (const auto &pin : gate.inputPins) {*/
        /*                                                        bool pinState = false;*/
        /*                                                        for (const auto &conn : pin) {*/
        /*                                                            if (registry.valid(conn.first)) {*/
        /*                                                                auto &srcGate = registry.get<GateComponent>(conn.first);*/
        /*                                                                if (!srcGate.outputStates.empty()) {*/
        /*                                                                    pinState = pinState || srcGate.outputStates[0];*/
        /*                                                                }*/
        /*                                                            }*/
        /*                                                        }*/
        /*                                                        pinValues.push_back(pinState);*/
        /*                                                    }*/
        /*                                                    bool newState = (pinValues.size() >= 2) ? !(pinValues[0] ^ pinValues[1]) : false;*/
        /*                                                    bool changed = false;*/
        /*                                                    for (auto state : gate.outputStates) {*/
        /*                                                        if (state != newState) {*/
        /*                                                            state = newState;*/
        /*                                                            changed = true;*/
        /*                                                        }*/
        /*                                                    }*/
        /*                                                    return changed; }, SimDelayMilliSeconds(100)));*/
        /**/
        /*auto nandGate = ComponentDefinition(ComponentType::NAND, "NAND Gate", "Digital Gates", 2, 1, [](entt::registry &registry, entt::entity e) -> bool {*/
        /*                                   auto &gate = registry.get<GateComponent>(e);*/
        /*                                   std::vector<bool> pinValues;*/
        /*                                   for (const auto &pin : gate.inputPins) {*/
        /*                                       bool pinState = false;*/
        /*                                       for (const auto &conn : pin) {*/
        /*                                           if (registry.valid(conn.first)) {*/
        /*                                               auto &srcGate = registry.get<GateComponent>(conn.first);*/
        /*                                               if (!srcGate.outputStates.empty()) {*/
        /*                                                   pinState = pinState || srcGate.outputStates[conn.second];*/
        /*                                                   if (pinState)*/
        /*                                                       break;*/
        /*                                               }*/
        /*                                           }*/
        /*                                       }*/
        /*                                       pinValues.push_back(pinState);*/
        /*                                   }*/
        /*                                   bool newState = true;*/
        /*                                   for (auto state : pinValues) {*/
        /*                                       newState = newState && state;*/
        /*                                       if (!newState)*/
        /*                                           break;*/
        /*                                   }*/
        /*newState = !newState;*/
        /**/
        /*                                   bool changed = false;*/
        /*                                   for (auto state : gate.outputStates) {*/
        /*                                       if (state != newState) {*/
        /*                                           state = newState;*/
        /*                                           changed = true;*/
        /*                                       }*/
        /*                                   }*/
        /*                                   return changed; }, SimDelayMilliSeconds(100));*/
        /*nandGate.addModifiableProperty(Properties::ComponentProperty::inputCount, {3, 4, 5});*/
        /*ComponentCatalog::instance().registerComponent(nandGate);*/
        /**/
        /*ComponentCatalog::instance().registerComponent(ComponentDefinition(ComponentType::NOR, "NOR Gate", "Digital Gates", 2, 1, [](entt::registry &registry, entt::entity e) -> bool {*/
        /*                                                    auto &gate = registry.get<GateComponent>(e);*/
        /*                                                    std::vector<bool> pinValues;*/
        /*                                                    for (const auto &pin : gate.inputPins) {*/
        /*                                                        bool pinState = false;*/
        /*                                                        for (const auto &conn : pin) {*/
        /*                                                            if (registry.valid(conn.first)) {*/
        /*                                                                auto &srcGate = registry.get<GateComponent>(conn.first);*/
        /*                                                                if (!srcGate.outputStates.empty()) {*/
        /*                                                                    pinState = pinState || srcGate.outputStates[0];*/
        /*                                                                }*/
        /*                                                            }*/
        /*                                                        }*/
        /*                                                        pinValues.push_back(pinState);*/
        /*                                                    }*/
        /*                                                    bool newState = (pinValues.size() >= 2) ? !(pinValues[0] || pinValues[1]) : false;*/
        /*                                                    bool changed = false;*/
        /*                                                    for (auto state : gate.outputStates) {*/
        /*                                                        if (state != newState) {*/
        /*                                                            state = newState;*/
        /*                                                            changed = true;*/
        /*                                                        }*/
        /*                                                    }*/
        /*                                                    return changed; }, SimDelayMilliSeconds(100)));*/
    }
} // namespace Bess::SimEngine
