#pragma once

#include "simulation_engine.h"

namespace Bess::SimEngine {
    class SimEngineSerializer {
      public:
        SimEngineSerializer(const SimulationEngine &engine);
        void serializeToPath(const std::string &path);
    };
} // namespace Bess::SimEngine
