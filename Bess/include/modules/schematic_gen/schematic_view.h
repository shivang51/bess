#pragma once

#include "entt/entt.hpp"
#include <vector>

namespace Bess::Modules::SchematicGen {
    enum GraphNodeType {
        unknown,
        input,
        output,
        other
    };

    struct GraphNode {
        uint64_t id = -1;
        GraphNodeType type = GraphNodeType::unknown;
        std::vector<uint64_t> connections = {};
    };

    class SchematicView {
      public:
        static SchematicView &getInstance();

        SchematicView() = default;
        void generateDiagram(const entt::registry &registry);

      private:
        void generateCompGraph(const entt::registry &registry);
    };
} // namespace Bess::Modules::SchematicGen
