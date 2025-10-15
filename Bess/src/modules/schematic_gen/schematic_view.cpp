#include "scene/scene_pch.h"
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
        generateTransform();
    }

    void SchematicView::generateTransform() {
        BESS_TRACE("[SchemeticGen] Generating transform");
        m_transforms = {};
        const size_t levelsCount = m_levels.size();

        size_t x = 0, y = 0;
        size_t W = 600, H = 600;
        const float dx = (float)W / (levelsCount + 1);

        for (size_t i = 0; i < m_levels.size(); i++) {
            x = dx * (i + 1);
            const float dh = (float)H / (m_levels[i].size() + 1);
            for (size_t j = 0; j < m_levels[i].size(); j++) {
                GraphNode *node = m_levels[i][j];
                y = dh * (j + 1);
                m_transforms[node->id] = glm::vec2(x, y);
                BESS_TRACE("{} -> {}, {}", node->id, x, y);
            }
        }
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
        }

        BESS_TRACE("[SchemeticGen] Found {} input nodes", m_graph.size());


        if (m_graph.empty()) {
            BESS_INFO("[SchemeticGen] Exiting due to no input nodes");
            return;
        }

        BESS_TRACE("[SchemeticGen] Generating Levels");

        // adding input nodes as level 0
        m_levels.emplace_back(m_graph);
        BESS_TRACE("[SchemeticGen] Level 0 has {} nodes", m_graph.size());

        auto &simEngine = SimEngine::SimulationEngine::instance();
        std::vector<uint64_t> processedNodes = {};

        while (!nodes.empty()) {
            size_t n = nodes.size();
            std::vector<GraphNode *> level = {};
            auto simComponentView = m_registry.view<SceneComponent::SimulationComponent>();
            while (n--) {
                const auto node = nodes.front();
                nodes.pop();

                for (auto& conn : node->outputs) {

                    if (std::find(processedNodes.begin(), processedNodes.end(), conn.gateEnt) != processedNodes.end()) {
                        continue;
                    }

                    processedNodes.emplace_back(conn.gateEnt);

                    const entt::entity entity = (entt::entity)conn.gateEnt;
                    auto connections = getConnectionsForEntity(entity);

                    if (connections.second.empty() && connections.first == 0) {
                        BESS_WARN("[SchematicGen] Ignoring component {}, due to zero connections", conn.gateEnt);
                    }

                    auto &simComp = simComponentView.get<SceneComponent::SimulationComponent>(entity);
                    const auto compType = simEngine.getComponentType(simComp.simEngineEntity);

                    const auto type = compType == SimEngine::ComponentType::OUTPUT ? GraphNodeType::output : GraphNodeType::other;

                    auto newNode = new GraphNode({conn.gateEnt,
                                                  type,
                                                  connections.first, // input should have any input pins
                                                  connections.second});

                    nodes.emplace(newNode);
                    m_graph.emplace_back(newNode);
                    level.emplace_back(newNode);
                }
            }

            if (level.empty())
                continue;
			
            BESS_TRACE("[SchematicGen] Level {} has {} nodes", m_levels.size(), level.size());
            m_levels.emplace_back(level);
        }

        BESS_TRACE("[SchematicGen] Found {} nodes accross {} levels", m_graph.size(), m_levels.size());
    }

    std::pair<size_t, std::vector<Connection>> SchematicView::getConnectionsForEntity(entt::entity ent) const {
        BESS_ASSERT(m_registry.all_of<SceneComponent::SimulationComponent>(ent), "Entity should have a SimulationComponent to find connections");
        static auto &simEngine = SimEngine::SimulationEngine::instance();
        auto &simComp = m_registry.get<SceneComponent::SimulationComponent>(ent);
        const Bess::SimEngine::ConnectionBundle connPair = simEngine.getConnections(simComp.simEngineEntity);
        std::vector<Connection> connections = {};
        for (const auto& conn : connPair.outputs) {
            for (auto &[gateUUID, pinIdx] : conn) {
                const auto ent_ = (uint64_t)m_sceneRef.getSceneEntityFromSimUuid(gateUUID);
                connections.push_back(Connection{ent_, pinIdx});
            }
        }
        return {connPair.inputs.size(), connections};
    }
} // namespace Bess::Modules::SchematicGen
