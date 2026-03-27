#pragma once

#include "bverilog/sim_engine_importer.h"

namespace Bess {
    namespace Canvas {
        class Scene;
    }
    namespace SimEngine {
        class SimulationEngine;
    }
}

namespace Bess::Pages {
    void populateSceneFromVerilogImportResult(const Verilog::SimEngineImportResult &result,
                                              SimEngine::SimulationEngine &simEngine,
                                              Canvas::Scene &scene);
}
