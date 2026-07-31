#pragma once

#include "common/bess_api.h"
#include "common/bess_uuid.h"

#include <cstddef>
#include <memory>
#include <string>

namespace Bess {
    class SceneDriver;

    namespace Canvas {
        class SceneComponent;
    }

    namespace SimEngine {
        class SimulationEngine;
    }
} // namespace Bess

namespace Bess::UI::Proj {
    struct Res {
        bool ok = false;
        std::string msg;

        explicit operator bool() const noexcept {
            return ok;
        }
    };

    struct LayoutRes {
        bool applied = false;
        std::size_t count = 0;
    };

    BESS_API SceneDriver &scenes();
    BESS_API SimEngine::SimulationEngine &sim();
    BESS_API Res newProj();
    BESS_API Res addConn(std::shared_ptr<Canvas::SceneComponent> conn,
                         UUID scene = UUID::null);
    BESS_API LayoutRes layout();
} // namespace Bess::UI::Proj
