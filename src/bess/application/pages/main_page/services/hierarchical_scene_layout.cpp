#include "pages/main_page/services/hierarchical_scene_layout.h"

#include "pages/main_page/scene_components/module_scene_component.h"
#include "pages/main_page/scene_components/sim_scene_component.h"
#include "pages/main_page/scene_components/slot_scene_component.h"
#include "scene/scene.h"
#include "simulation_engine.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace Bess::Pages {
    namespace {
        using namespace Bess::Canvas;
        using namespace Bess::SimEngine;

        template <typename T>
        struct PairHash {
            size_t operator()(const std::pair<T, T> &value) const noexcept {
                const auto lhs = std::hash<T>{}(value.first);
                const auto rhs = std::hash<T>{}(value.second);
                return lhs ^ (rhs << 1);
            }
        };

        struct LayoutNode {
            std::shared_ptr<SimulationSceneComponent> component;
            UUID simId = UUID::null;
            std::string name;
            float width = 120.f;
            float height = 96.f;
            float currentY = 0.f;
            float provisionalY = 0.f;
            int rank = 0;
            bool boundaryInput = false;
            bool boundaryOutput = false;
            std::unordered_map<size_t, int> incoming;
            std::unordered_map<size_t, int> outgoing;
        };

        struct MetaNode {
            std::vector<size_t> members;
            std::unordered_map<int, int> incoming;
            std::unordered_map<int, int> outgoing;
            std::string stableName;
            uint64_t stableId = 0;
            int rank = 0;
            bool hasBoundaryInput = false;
            bool hasBoundaryOutput = false;
        };

        uint64_t uuidKey(const UUID &uuid) {
            return static_cast<uint64_t>(uuid);
        }

        float normalizeExtent(float value, float fallback) {
            return value > 0.f ? value : fallback;
        }

        glm::vec2 estimateComponentSize(const std::shared_ptr<SimulationSceneComponent> &component,
                                       const SceneState &sceneState) {
            const auto &transform = component->getTransform();
            if (transform.scale.x > 0.f && transform.scale.y > 0.f) {
                return transform.scale;
            }

            size_t inputCount = 0;
            for (const auto &slotId : component->getInputSlots()) {
                const auto slot = sceneState.getComponentByUuid<SlotSceneComponent>(slotId);
                if (slot && !slot->isResizeSlot()) {
                    ++inputCount;
                }
            }

            size_t outputCount = 0;
            for (const auto &slotId : component->getOutputSlots()) {
                const auto slot = sceneState.getComponentByUuid<SlotSceneComponent>(slotId);
                if (slot && !slot->isResizeSlot()) {
                    ++outputCount;
                }
            }

            const auto longestSide = std::max(inputCount, outputCount);
            const auto estimatedWidth =
                std::max(120.f, 28.f + static_cast<float>(component->getName().size()) * 9.f);
            const auto estimatedHeight =
                52.f + static_cast<float>(std::max<size_t>(1, longestSide)) * 32.f;
            return {estimatedWidth, estimatedHeight};
        }

        int nodePriority(const LayoutNode &node) {
            if (node.boundaryInput) {
                return 0;
            }
            if (node.boundaryOutput) {
                return 2;
            }
            return 1;
        }

        std::vector<LayoutNode> collectLayoutNodes(Scene &scene) {
            auto &sceneState = scene.getState();
            std::vector<LayoutNode> nodes;
            nodes.reserve(sceneState.getRootComponents().size());

            for (const auto &rootId : sceneState.getRootComponents()) {
                const auto component = sceneState.getComponentByUuid(rootId);
                if (!component) {
                    continue;
                }

                if (component->getType() != SceneComponentType::simulation &&
                    component->getType() != SceneComponentType::module) {
                    continue;
                }

                const auto simComponent =
                    std::dynamic_pointer_cast<SimulationSceneComponent>(component);
                if (!simComponent || simComponent->getSimEngineId() == UUID::null) {
                    continue;
                }

                LayoutNode node;
                node.component = simComponent;
                node.simId = simComponent->getSimEngineId();
                node.name = simComponent->getName();
                node.currentY = simComponent->getTransform().position.y;
                const auto estimatedSize = estimateComponentSize(simComponent, sceneState);
                node.width = normalizeExtent(estimatedSize.x, 120.f);
                node.height = normalizeExtent(estimatedSize.y, 96.f);

                const auto componentDef = simComponent->getCompDef();
                if (componentDef) {
                    node.boundaryInput =
                        componentDef->getBehaviorType() == ComponentBehaviorType::input;
                    node.boundaryOutput =
                        componentDef->getBehaviorType() == ComponentBehaviorType::output;
                }

                nodes.push_back(std::move(node));
            }

            std::ranges::sort(nodes, [](const LayoutNode &lhs, const LayoutNode &rhs) {
                if (lhs.currentY != rhs.currentY) {
                    return lhs.currentY < rhs.currentY;
                }

                const auto lhsPriority = nodePriority(lhs);
                const auto rhsPriority = nodePriority(rhs);
                if (lhsPriority != rhsPriority) {
                    return lhsPriority < rhsPriority;
                }

                if (lhs.name != rhs.name) {
                    return lhs.name < rhs.name;
                }

                return uuidKey(lhs.simId) < uuidKey(rhs.simId);
            });

            return nodes;
        }

        size_t buildEdges(std::vector<LayoutNode> &nodes,
                          SimulationEngine &simEngine) {
            std::unordered_map<UUID, size_t> indexBySimId;
            indexBySimId.reserve(nodes.size());

            for (size_t i = 0; i < nodes.size(); ++i) {
                indexBySimId[nodes[i].simId] = i;
            }

            std::unordered_map<std::pair<size_t, size_t>, int, PairHash<size_t>> edgeWeights;
            for (size_t i = 0; i < nodes.size(); ++i) {
                const auto connections = simEngine.getConnections(nodes[i].simId);
                for (const auto &slotConnections : connections.outputs) {
                    for (const auto &[dstId, dstSlot] : slotConnections) {
                        (void)dstSlot;

                        const auto dstIt = indexBySimId.find(dstId);
                        if (dstIt == indexBySimId.end() || dstIt->second == i) {
                            continue;
                        }

                        edgeWeights[{i, dstIt->second}] += 1;
                    }
                }
            }

            for (const auto &[edge, weight] : edgeWeights) {
                nodes[edge.first].outgoing[edge.second] += weight;
                nodes[edge.second].incoming[edge.first] += weight;
            }

            return edgeWeights.size();
        }

        std::vector<int> computeSccIndices(const std::vector<LayoutNode> &nodes,
                                           std::vector<std::vector<size_t>> &sccs) {
            std::vector<int> discovery(nodes.size(), -1);
            std::vector<int> lowLink(nodes.size(), -1);
            std::vector<bool> onStack(nodes.size(), false);
            std::vector<size_t> stack;
            std::vector<int> sccIndex(nodes.size(), -1);
            int nextDiscovery = 0;

            std::function<void(size_t)> strongConnect = [&](size_t nodeIndex) {
                discovery[nodeIndex] = nextDiscovery;
                lowLink[nodeIndex] = nextDiscovery;
                ++nextDiscovery;

                stack.push_back(nodeIndex);
                onStack[nodeIndex] = true;

                for (const auto &[neighborIndex, weight] : nodes[nodeIndex].outgoing) {
                    (void)weight;

                    if (discovery[neighborIndex] == -1) {
                        strongConnect(neighborIndex);
                        lowLink[nodeIndex] =
                            std::min(lowLink[nodeIndex], lowLink[neighborIndex]);
                    } else if (onStack[neighborIndex]) {
                        lowLink[nodeIndex] =
                            std::min(lowLink[nodeIndex], discovery[neighborIndex]);
                    }
                }

                if (lowLink[nodeIndex] != discovery[nodeIndex]) {
                    return;
                }

                sccs.emplace_back();
                const int currentScc = static_cast<int>(sccs.size() - 1);
                while (!stack.empty()) {
                    const auto member = stack.back();
                    stack.pop_back();
                    onStack[member] = false;
                    sccIndex[member] = currentScc;
                    sccs.back().push_back(member);
                    if (member == nodeIndex) {
                        break;
                    }
                }
            };

            for (size_t i = 0; i < nodes.size(); ++i) {
                if (discovery[i] == -1) {
                    strongConnect(i);
                }
            }

            return sccIndex;
        }

        std::vector<MetaNode> buildMetaGraph(const std::vector<LayoutNode> &nodes,
                                             const std::vector<int> &sccIndex,
                                             const std::vector<std::vector<size_t>> &sccs) {
            std::vector<MetaNode> metaNodes(sccs.size());

            for (size_t i = 0; i < sccs.size(); ++i) {
                auto &metaNode = metaNodes[i];
                metaNode.members = sccs[i];

                uint64_t stableId = 0;
                bool hasStableId = false;
                for (const auto memberIndex : sccs[i]) {
                    const auto &node = nodes[memberIndex];
                    metaNode.hasBoundaryInput = metaNode.hasBoundaryInput || node.boundaryInput;
                    metaNode.hasBoundaryOutput = metaNode.hasBoundaryOutput || node.boundaryOutput;

                    if (metaNode.stableName.empty() || node.name < metaNode.stableName) {
                        metaNode.stableName = node.name;
                    }

                    const auto currentId = uuidKey(node.simId);
                    if (!hasStableId || currentId < stableId) {
                        stableId = currentId;
                        hasStableId = true;
                    }
                }

                metaNode.stableId = stableId;
            }

            for (size_t nodeIndex = 0; nodeIndex < nodes.size(); ++nodeIndex) {
                const auto srcMeta = sccIndex[nodeIndex];
                for (const auto &[neighborIndex, weight] : nodes[nodeIndex].outgoing) {
                    const auto dstMeta = sccIndex[neighborIndex];
                    if (srcMeta == dstMeta) {
                        continue;
                    }

                    metaNodes[srcMeta].outgoing[dstMeta] += weight;
                    metaNodes[dstMeta].incoming[srcMeta] += weight;
                }
            }

            return metaNodes;
        }

        std::vector<int> buildTopologicalOrder(const std::vector<MetaNode> &metaNodes) {
            std::vector<int> indegree(metaNodes.size(), 0);
            for (size_t i = 0; i < metaNodes.size(); ++i) {
                indegree[i] = static_cast<int>(metaNodes[i].incoming.size());
            }

            const auto sortMeta = [&](std::vector<int> &metaIds) {
                std::ranges::sort(metaIds, [&](int lhs, int rhs) {
                    if (metaNodes[lhs].stableName != metaNodes[rhs].stableName) {
                        return metaNodes[lhs].stableName < metaNodes[rhs].stableName;
                    }
                    return metaNodes[lhs].stableId < metaNodes[rhs].stableId;
                });
            };

            std::vector<int> ready;
            ready.reserve(metaNodes.size());
            for (size_t i = 0; i < metaNodes.size(); ++i) {
                if (indegree[i] == 0) {
                    ready.push_back(static_cast<int>(i));
                }
            }

            sortMeta(ready);

            std::vector<int> order;
            order.reserve(metaNodes.size());

            while (!ready.empty()) {
                const auto current = ready.front();
                ready.erase(ready.begin());
                order.push_back(current);

                for (const auto &[next, weight] : metaNodes[current].outgoing) {
                    (void)weight;
                    indegree[next] -= 1;
                    if (indegree[next] == 0) {
                        ready.push_back(next);
                    }
                }

                sortMeta(ready);
            }

            if (order.size() == metaNodes.size()) {
                return order;
            }

            for (size_t i = 0; i < metaNodes.size(); ++i) {
                if (std::ranges::find(order, static_cast<int>(i)) == order.end()) {
                    order.push_back(static_cast<int>(i));
                }
            }

            return order;
        }

        void assignRanks(std::vector<LayoutNode> &nodes,
                         std::vector<MetaNode> &metaNodes) {
            if (nodes.empty()) {
                return;
            }

            const auto topologicalOrder = buildTopologicalOrder(metaNodes);
            std::vector<int> longestPath(metaNodes.size(), 0);

            for (const auto metaIndex : topologicalOrder) {
                for (const auto &[nextMeta, weight] : metaNodes[metaIndex].outgoing) {
                    (void)weight;
                    longestPath[nextMeta] =
                        std::max(longestPath[nextMeta], longestPath[metaIndex] + 1);
                }
            }

            int maxRank = 0;
            for (const auto rank : longestPath) {
                maxRank = std::max(maxRank, rank);
            }

            for (size_t i = 0; i < metaNodes.size(); ++i) {
                auto &metaNode = metaNodes[i];
                const bool isIsolated = metaNode.incoming.empty() && metaNode.outgoing.empty();

                int rank = longestPath[i];
                if (metaNode.hasBoundaryInput || metaNode.incoming.empty()) {
                    rank = 0;
                }

                if (metaNode.hasBoundaryOutput ||
                    (!metaNode.outgoing.empty() ? false : !isIsolated)) {
                    rank = std::max(rank, maxRank);
                }

                metaNode.rank = rank;
            }

            std::vector<int> uniqueRanks;
            uniqueRanks.reserve(metaNodes.size());
            for (const auto &metaNode : metaNodes) {
                uniqueRanks.push_back(metaNode.rank);
            }
            std::ranges::sort(uniqueRanks);
            uniqueRanks.erase(std::unique(uniqueRanks.begin(), uniqueRanks.end()),
                              uniqueRanks.end());

            for (auto &metaNode : metaNodes) {
                metaNode.rank = static_cast<int>(
                    std::ranges::find(uniqueRanks, metaNode.rank) - uniqueRanks.begin());
                for (const auto member : metaNode.members) {
                    nodes[member].rank = metaNode.rank;
                }
            }
        }

        std::vector<std::vector<size_t>> buildLayers(const std::vector<LayoutNode> &nodes) {
            int maxRank = 0;
            for (const auto &node : nodes) {
                maxRank = std::max(maxRank, node.rank);
            }

            std::vector<std::vector<size_t>> layers(static_cast<size_t>(maxRank + 1));
            for (size_t i = 0; i < nodes.size(); ++i) {
                layers[nodes[i].rank].push_back(i);
            }

            for (auto &layer : layers) {
                std::ranges::sort(layer, [&](size_t lhs, size_t rhs) {
                    const auto &lhsNode = nodes[lhs];
                    const auto &rhsNode = nodes[rhs];
                    if (lhsNode.currentY != rhsNode.currentY) {
                        return lhsNode.currentY < rhsNode.currentY;
                    }

                    const auto lhsPriority = nodePriority(lhsNode);
                    const auto rhsPriority = nodePriority(rhsNode);
                    if (lhsPriority != rhsPriority) {
                        return lhsPriority < rhsPriority;
                    }

                    if (lhsNode.name != rhsNode.name) {
                        return lhsNode.name < rhsNode.name;
                    }

                    return uuidKey(lhsNode.simId) < uuidKey(rhsNode.simId);
                });
            }

            return layers;
        }

        void assignLayerYPositions(const std::vector<size_t> &layer,
                                   std::vector<LayoutNode> &nodes,
                                   float rowSpacing) {
            if (layer.empty()) {
                return;
            }

            float totalHeight = 0.f;
            for (const auto nodeIndex : layer) {
                totalHeight += nodes[nodeIndex].height;
            }
            totalHeight += rowSpacing * static_cast<float>(layer.size() - 1);

            float cursorY = -totalHeight / 2.f;
            for (const auto nodeIndex : layer) {
                cursorY += nodes[nodeIndex].height / 2.f;
                nodes[nodeIndex].provisionalY = cursorY;
                cursorY += nodes[nodeIndex].height / 2.f + rowSpacing;
            }
        }

        std::optional<float> computeIdealY(const LayoutNode &node,
                                           const std::vector<LayoutNode> &nodes,
                                           bool useIncoming) {
            const auto &edges = useIncoming ? node.incoming : node.outgoing;
            float weightedSum = 0.f;
            float totalWeight = 0.f;

            for (const auto &[neighborIndex, edgeWeight] : edges) {
                const auto &neighbor = nodes[neighborIndex];
                if (neighbor.rank == node.rank) {
                    continue;
                }

                const auto rankDistance =
                    std::max(1, std::abs(neighbor.rank - node.rank));
                const auto weight =
                    static_cast<float>(edgeWeight) / static_cast<float>(rankDistance);
                weightedSum += neighbor.provisionalY * weight;
                totalWeight += weight;
            }

            if (totalWeight <= 0.f) {
                return std::nullopt;
            }

            return weightedSum / totalWeight;
        }

        void reorderLayer(std::vector<size_t> &layer,
                          std::vector<LayoutNode> &nodes,
                          const HierarchicalSceneLayoutOptions &options,
                          bool useIncoming) {
            if (layer.size() < 2) {
                assignLayerYPositions(layer, nodes, options.rowSpacing);
                return;
            }

            std::unordered_map<size_t, size_t> previousOrder;
            previousOrder.reserve(layer.size());
            std::unordered_map<size_t, std::optional<float>> idealYByNode;
            idealYByNode.reserve(layer.size());

            for (size_t i = 0; i < layer.size(); ++i) {
                previousOrder[layer[i]] = i;
                idealYByNode[layer[i]] = computeIdealY(nodes[layer[i]], nodes, useIncoming);
            }

            std::stable_sort(layer.begin(), layer.end(), [&](size_t lhs, size_t rhs) {
                const auto &lhsIdeal = idealYByNode.at(lhs);
                const auto &rhsIdeal = idealYByNode.at(rhs);

                if (lhsIdeal.has_value() && rhsIdeal.has_value()) {
                    if (std::abs(*lhsIdeal - *rhsIdeal) > 0.5f) {
                        return *lhsIdeal < *rhsIdeal;
                    }
                } else if (lhsIdeal.has_value() != rhsIdeal.has_value()) {
                    return lhsIdeal.has_value();
                }

                if (previousOrder.at(lhs) != previousOrder.at(rhs)) {
                    return previousOrder.at(lhs) < previousOrder.at(rhs);
                }

                const auto lhsPriority = nodePriority(nodes[lhs]);
                const auto rhsPriority = nodePriority(nodes[rhs]);
                if (lhsPriority != rhsPriority) {
                    return lhsPriority < rhsPriority;
                }

                if (nodes[lhs].name != nodes[rhs].name) {
                    return nodes[lhs].name < nodes[rhs].name;
                }

                return uuidKey(nodes[lhs].simId) < uuidKey(nodes[rhs].simId);
            });

            assignLayerYPositions(layer, nodes, options.rowSpacing);
        }

        std::vector<float> computeLayerCenters(const std::vector<std::vector<size_t>> &layers,
                                               const std::vector<LayoutNode> &nodes,
                                               float layerSpacing) {
            std::vector<float> layerWidths(layers.size(), 120.f);
            for (size_t i = 0; i < layers.size(); ++i) {
                float width = 120.f;
                for (const auto nodeIndex : layers[i]) {
                    width = std::max(width, nodes[nodeIndex].width);
                }
                layerWidths[i] = width;
            }

            std::vector<float> centers(layers.size(), 0.f);
            if (layers.empty()) {
                return centers;
            }

            centers[0] = layerWidths[0] / 2.f;
            for (size_t i = 1; i < layers.size(); ++i) {
                centers[i] = centers[i - 1] +
                             (layerWidths[i - 1] / 2.f) +
                             layerSpacing +
                             (layerWidths[i] / 2.f);
            }

            const auto minX = centers.front() - (layerWidths.front() / 2.f);
            const auto maxX = centers.back() + (layerWidths.back() / 2.f);
            const auto offset = (minX + maxX) / 2.f;

            for (auto &center : centers) {
                center -= offset;
            }

            return centers;
        }
    } // namespace

    HierarchicalSceneLayoutResult applyHierarchicalSceneLayout(
        Canvas::Scene &scene,
        SimEngine::SimulationEngine &simEngine,
        const HierarchicalSceneLayoutOptions &options) {
        HierarchicalSceneLayoutResult result;

        auto nodes = collectLayoutNodes(scene);
        result.laidOutNodes = nodes.size();
        if (nodes.size() < 2) {
            result.applied = !nodes.empty();
            return result;
        }

        result.uniqueEdges = buildEdges(nodes, simEngine);
        if (result.uniqueEdges == 0) {
            result.applied = false;
            return result;
        }

        std::vector<std::vector<size_t>> sccs;
        const auto sccIndex = computeSccIndices(nodes, sccs);
        auto metaNodes = buildMetaGraph(nodes, sccIndex, sccs);
        assignRanks(nodes, metaNodes);

        auto layers = buildLayers(nodes);
        for (auto &layer : layers) {
            assignLayerYPositions(layer, nodes, options.rowSpacing);
        }

        const auto sweepCount = std::max(1, options.crossingReductionPasses);
        for (int pass = 0; pass < sweepCount; ++pass) {
            for (size_t rank = 1; rank < layers.size(); ++rank) {
                reorderLayer(layers[rank], nodes, options, true);
            }

            for (size_t rank = layers.size(); rank-- > 0;) {
                if (rank + 1 >= layers.size()) {
                    continue;
                }
                reorderLayer(layers[rank], nodes, options, false);
            }
        }

        const auto layerCenters =
            computeLayerCenters(layers, nodes, options.layerSpacing);

        for (const auto &layer : layers) {
            for (const auto nodeIndex : layer) {
                auto &component = nodes[nodeIndex].component;
                auto newPosition = component->getTransform().position;
                if (newPosition.z == 0.f) {
                    newPosition.z = scene.getNextZCoord();
                }
                newPosition.x = layerCenters[nodes[nodeIndex].rank];
                newPosition.y = nodes[nodeIndex].provisionalY;
                component->setPosition(newPosition);

                auto schematicTransform = component->getSchematicTransform();
                if (schematicTransform.position.z == 0.f) {
                    schematicTransform.position.z = newPosition.z;
                }
                schematicTransform.position.x = newPosition.x;
                schematicTransform.position.y = newPosition.y;
                component->setSchematicTransform(schematicTransform);
            }
        }

        result.applied = true;
        return result;
    }
} // namespace Bess::Pages
