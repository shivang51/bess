#include "modules/schematic_gen/schematic_view.h"
#include "common/log.h"
#include "scene/components/components.h"
#include "scene/scene.h"
#include <queue>

namespace SceneComponent = Bess::Canvas::Components;

namespace Bess::Modules::SchematicGen {

    SchematicView::SchematicView(const Canvas::Scene &sceneRef, const entt::registry &registry) : m_sceneRef(sceneRef), m_registry(registry) {}

    void SchematicView::generateDiagram() {
        BESS_INFO("[SchemeticGen] Reading registry");
        generateCompGraphAndLevels();
    }

    void SchematicView::generateCompGraphAndLevels() {
        BESS_TRACE("[SchemeticGen] Generating component graph");

        std::queue<GraphNode *> nodes; // used for forming levels

        for (auto entity : m_registry.view<SceneComponent::SimulationInputComponent>()) {
            auto connections = getConnectionsForEntity(entity);

            if (connections.second.size() == 0) {
                BESS_WARN("[SchematicGen] Ignoring input {}, due to zero connections", (uint64_t)entity);
                continue;
            }

            auto node = new GraphNode({(uint64_t)entity,
                                       GraphNodeType::input,
                                       0, // input should have any input pins
                                       connections.second});
            m_graph.emplace_back(node);
            nodes.emplace(node);
        };

        BESS_TRACE("[SchemeticGen] Found {} input nodes", m_graph.size());

        if (m_graph.size() == 0) {
            BESS_INFO("[SchemeticGen] Exiting due to no input nodes");
            return;
        }

        BESS_TRACE("[SchemeticGen] Generating Levels");

        auto &simEngine = SimEngine::SimulationEngine::instance();
        std::vector<uint64_t> processedNodes = {};

        while (!nodes.empty()) {
            size_t n = nodes.size();
            std::vector<GraphNode *> level = {};
            while (n--) {
                auto node = nodes.front();
                nodes.pop();

                for (auto conn : node->outputs) {
                    entt::entity entity = conn.gateEnt;

                    if (std::find(processedNodes.begin(), processedNodes.end(), (uint64_t)entity) != processedNodes.end()) {
                        continue;
                    }

                    processedNodes.emplace_back((uint64_t)entity);

                    auto connections = getConnectionsForEntity(entity);

                    if (connections.second.size() == 0 && connections.first == 0) {
                        BESS_WARN("[SchematicGen] Ignoring component {}, due to zero connections", (uint64_t)entity);
                    }

                    auto simComp = m_registry.get<SceneComponent::SimulationComponent>(entity);
                    auto compType = simEngine.getComponentType(simComp.simEngineEntity);

                    auto type = compType == SimEngine::ComponentType::OUTPUT ? GraphNodeType::output : GraphNodeType::other;

                    auto newNode = new GraphNode({(uint64_t)entity,
                                                  GraphNodeType::other,
                                                  connections.first, // input should have any input pins
                                                  connections.second});

                    nodes.emplace(newNode);
                    m_graph.emplace_back(newNode);
                    level.emplace_back(newNode);
                }
            }
            m_levels.emplace_back(level);
        }

        BESS_TRACE("[SchematicGen] Found {} nodes accross {} levels", m_graph.size(), m_levels.size());
    }

    std::pair<int, std::vector<Connection>> SchematicView::getConnectionsForEntity(entt::entity ent) {
        std::pair<int, std::vector<Connection>> connections = {0, {}};
        BESS_ASSERT(m_registry.all_of<SceneComponent::SimulationComponent>(ent), "Entity should have a SimulationComponent to find connections");
        static auto &simEngine = SimEngine::SimulationEngine::instance();
        auto simComp = m_registry.get<SceneComponent::SimulationComponent>(ent);
        auto simConns = simEngine.getConnections(simComp.simEngineEntity);
        connections.first = simConns.first.size();
        for (auto conn : simConns.second) {
            for (int i = 0; i < (int)conn.size(); i++) {
                auto ent = m_sceneRef.getSceneEntityFromSimUuid(conn[i].first);
                connections.second.emplace_back(Connection{ent, i});
            }
        }
        return connections;
    }
} // namespace Bess::Modules::SchematicGen
