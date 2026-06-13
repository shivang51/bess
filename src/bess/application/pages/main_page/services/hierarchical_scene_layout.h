#pragma once

#include <cstddef>

namespace Bess {
    namespace Canvas {
        class Scene;
    }
    namespace SimEngine {
        class SimulationEngine;
    }
} // namespace Bess

namespace Bess::Pages {
    struct HierarchicalSceneLayoutOptions {
        float layerSpacing = 220.f;
        float rowSpacing = 72.f;
        int crossingReductionPasses = 6;
    };

    struct HierarchicalSceneLayoutResult {
        size_t laidOutNodes = 0;
        size_t uniqueEdges = 0;
        bool applied = false;
    };

    HierarchicalSceneLayoutResult applyHierarchicalSceneLayout(
        Canvas::Scene &scene,
        SimEngine::SimulationEngine &simEngine,
        const HierarchicalSceneLayoutOptions &options = {});
} // namespace Bess::Pages
