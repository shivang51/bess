#pragma once

#include "entt/entt.hpp"
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
        entt::entity gateEnt;
        int pinIdx;
    };

    struct GraphNode {
        uint64_t id = -1;
        GraphNodeType type = GraphNodeType::unknown;
        int inputCount = 0;
        std::vector<Connection> outputs = {};
    };

    class SchematicView {
      public:
        SchematicView(const Canvas::Scene &sceneRef, const entt::registry &registry);
        void generateDiagram();

      private:
        void generateCompGraphAndLevels();
        std::pair<int, std::vector<Connection>> getConnectionsForEntity(entt::entity ent);

        std::vector<GraphNode *> m_graph;
        std::vector<std::vector<GraphNode *>> m_levels;

        const entt::registry &m_registry;
        const Canvas::Scene &m_sceneRef;
    };
} // namespace Bess::Modules::SchematicGen
