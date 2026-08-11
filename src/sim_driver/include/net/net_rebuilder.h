#pragma once

#include "common/types.h"
#include "net.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace Bess::SimEngine {

    /**
     * Rebuild component nets from reciprocal port connections.
     *
     * Existing net UUIDs are retained where possible. When an old net has
     * split, its largest connected component keeps the UUID and the remaining
     * components receive new UUIDs.
     */
    template <typename TComponent, typename TComponentsMap>
    void rebuildComponentNets(const TComponentsMap &components,
                              std::unordered_map<UUID, Net> &nets) {
        using ComponentPtr = std::shared_ptr<TComponent>;

        struct Group {
            std::vector<UUID> components;
            std::unordered_map<UUID, size_t> oldNetCounts;
        };

        std::unordered_map<UUID, ComponentPtr> typedComponents;
        typedComponents.reserve(components.size());
        for (const auto &[componentId, component] : components) {
            if (auto typed = std::dynamic_pointer_cast<TComponent>(component)) {
                typedComponents.emplace(componentId, std::move(typed));
            }
        }

        auto hasReciprocalConnection = [&](const UUID &componentId,
                                           size_t portIndex,
                                           const auto &otherPins,
                                           const ComponentPin &connection) {
            const auto &[otherId, otherPortIndex] = connection;
            const auto otherIt = typedComponents.find(otherId);
            if (otherIt == typedComponents.end() || otherPortIndex < 0) {
                return false;
            }

            const auto &reciprocalPins = otherPins(*otherIt->second);
            if (static_cast<size_t>(otherPortIndex) >= reciprocalPins.size()) {
                return false;
            }

            return std::ranges::any_of(
                reciprocalPins[otherPortIndex], [&](const auto &reciprocal) {
                    return reciprocal.first == componentId &&
                           reciprocal.second == static_cast<int>(portIndex);
                });
        };

        std::vector<Group> groups;
        std::unordered_set<UUID> visited;
        visited.reserve(typedComponents.size());

        for (const auto &[rootId, root] : typedComponents) {
            if (visited.contains(rootId)) {
                continue;
            }

            Group group;
            std::vector<UUID> pending{rootId};
            visited.insert(rootId);

            while (!pending.empty()) {
                const auto componentId = pending.back();
                pending.pop_back();

                const auto component = typedComponents.at(componentId);
                group.components.push_back(componentId);
                if (const auto oldNetId = component->getNetUuid();
                    oldNetId != UUID::null) {
                    ++group.oldNetCounts[oldNetId];
                }

                auto visitConnections = [&](const auto &pins,
                                            const auto &otherPins) {
                    for (size_t portIndex = 0; portIndex < pins.size();
                         ++portIndex) {
                        for (const auto &connection : pins[portIndex]) {
                            if (!hasReciprocalConnection(componentId,
                                                         portIndex,
                                                         otherPins,
                                                         connection)) {
                                continue;
                            }

                            const auto &otherId = connection.first;
                            if (visited.insert(otherId).second) {
                                pending.push_back(otherId);
                            }
                        }
                    }
                };

                visitConnections(component->getInputConnections(),
                                 [](const TComponent &other) -> const auto & {
                                     return other.getOutputConnections();
                                 });
                visitConnections(component->getOutputConnections(),
                                 [](const TComponent &other) -> const auto & {
                                     return other.getInputConnections();
                                 });
            }

            std::ranges::sort(group.components,
                              [](const UUID &lhs, const UUID &rhs) {
                                  return static_cast<uint64_t>(lhs) <
                                         static_cast<uint64_t>(rhs);
                              });
            groups.push_back(std::move(group));
        }

        std::ranges::sort(groups, [](const Group &lhs, const Group &rhs) {
            return static_cast<uint64_t>(lhs.components.front()) <
                   static_cast<uint64_t>(rhs.components.front());
        });

        // An old UUID may occur in several groups after a split. Reserve it
        // for the largest group, using the lowest component UUID as a stable
        // tie-breaker.
        std::unordered_map<UUID, size_t> oldNetOwners;
        for (size_t groupIndex = 0; groupIndex < groups.size(); ++groupIndex) {
            for (const auto &[oldNetId, _] : groups[groupIndex].oldNetCounts) {
                const auto ownerIt = oldNetOwners.find(oldNetId);
                if (ownerIt == oldNetOwners.end()) {
                    oldNetOwners.emplace(oldNetId, groupIndex);
                    continue;
                }

                const auto ownerIndex = ownerIt->second;
                if (groups[groupIndex].components.size() >
                    groups[ownerIndex].components.size()) {
                    ownerIt->second = groupIndex;
                }
            }
        }

        std::unordered_map<UUID, Net> rebuiltNets;
        rebuiltNets.reserve(groups.size());
        for (size_t groupIndex = 0; groupIndex < groups.size(); ++groupIndex) {
            auto &group = groups[groupIndex];
            UUID netId = UUID::null;
            size_t bestOldNetCount = 0;

            for (const auto &[oldNetId, count] : group.oldNetCounts) {
                const auto ownerIt = oldNetOwners.find(oldNetId);
                if (ownerIt == oldNetOwners.end() ||
                    ownerIt->second != groupIndex) {
                    continue;
                }

                if (netId == UUID::null || count > bestOldNetCount ||
                    (count == bestOldNetCount &&
                     static_cast<uint64_t>(oldNetId) <
                         static_cast<uint64_t>(netId))) {
                    netId = oldNetId;
                    bestOldNetCount = count;
                }
            }

            if (netId == UUID::null || rebuiltNets.contains(netId)) {
                do {
                    netId = UUID{};
                } while (netId == UUID::null || rebuiltNets.contains(netId));
            }

            Net net;
            net.setUUID(netId);
            net.setComponents(group.components);
            for (const auto &componentId : group.components) {
                typedComponents.at(componentId)->setNetUuid(netId);
            }
            rebuiltNets.emplace(netId, std::move(net));
        }

        nets = std::move(rebuiltNets);
    }

} // namespace Bess::SimEngine
