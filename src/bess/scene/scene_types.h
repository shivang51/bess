#pragma once

#include <cstdint>
#include <glm.hpp>

namespace Bess::Canvas {

    // UI viewport bounds in window coordinates. Scene input events are formed
    // from this so layers can work in scene coordinates.
    struct ViewportTransform {
        glm::vec2 pos{0.f};
        glm::vec2 size{0.f};
    };

    enum class SceneMode : uint8_t {
        general,
    };

    enum class SceneDrawMode : uint8_t {
        none,
        connection,
    };

    struct SelBoxContext {
        glm::vec2 start{0.f};
        glm::vec2 end{0.f};
        bool draw = false;
        bool queueSelInNextFrame = false;
        bool queueForSel = false;
    };

    struct PickingReadbackRequest {
        uint32_t x = 0;
        uint32_t y = 0;
        uint32_t width = 0;
        uint32_t height = 0;
        bool active = false;
    };
} // namespace Bess::Canvas
