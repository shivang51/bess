#pragma once

#include "entt/entt.hpp"
#include "glm.hpp"
#include <vector>

namespace Bess::Canvas {
    class Scene;
}

namespace Bess::Modules::SchematicGen {
    enum GraphNodeType {
        unknown,
        input,
        output,
        other
    };

    struct Connection {
        uint64_t gateEnt;
        int pinIdx;
    };

    struct GraphNode {
        uint64_t id = -1;
        GraphNodeType type = GraphNodeType::unknown;
        size_t inputCount = 0;
        std::vector<Connection> outputs = {};
    };

    class SchematicView {
      public:
        SchematicView(const Canvas::Scene &sceneRef, const entt::registry &registry);
        void generateDiagram();

      private:
        void generateCompGraphAndLevels();
        void generateTransform();
        std::pair<size_t, std::vector<Connection>> getConnectionsForEntity(entt::entity ent) const;

        std::vector<GraphNode *> m_graph;
        std::vector<std::vector<GraphNode *>> m_levels;
        std::unordered_map<uint64_t, glm::vec2> m_transforms;

        const entt::registry &m_registry;
        const Canvas::Scene &m_sceneRef;
    };
} // namespace Bess::Modules::SchematicGen
