#include "modules/schematic_gen/schematic_view.h"
#include "common/log.h"
#include "scene/components/components.h"

namespace SceneComponent = Bess::Canvas::Components;

namespace Bess::Modules::SchematicGen {
    SchematicView &SchematicView::getInstance() {
        static SchematicView instance;
        return instance;
    }

    void SchematicView::generateDiagram(const entt::registry &registry) {
        BESS_INFO("[SchemeticGen] Reading registry");
        generateCompGraph(registry);
    }

    void SchematicView::generateCompGraph(const entt::registry &registry) {
        BESS_TRACE("[SchemeticGen] Generating component graph");

        std::vector<GraphNode *> nodes;

        for (auto entity : registry.view<SceneComponent::SimulationInputComponent>()) {
            auto node = new GraphNode({(uint64_t)entity,
                                       GraphNodeType::input});
            nodes.emplace_back(node);
        };

        for (auto entity : registry.view<SceneComponent::ConnectionComponent>()) {
        };
    }
} // namespace Bess::Modules::SchematicGen
