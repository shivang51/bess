#pragma once
#include "glm.hpp"
#include "json/json.h"
#include <cstdint>

namespace Bess::Canvas {

    enum class SceneComponentTypeFlag : uint8_t {
        showInProjectExplorer = 1 << 7,
    };

    enum class SceneComponentType : int8_t {
        _base = -1,
        simulation = 0 | (int8_t)SceneComponentTypeFlag::showInProjectExplorer,
        slot = 1,
        nonSimulation = 2 | (int8_t)SceneComponentTypeFlag::showInProjectExplorer,
        connection = 3,
        connJoint = 4,
    };

    class Transform {
      public:
        Transform() = default;
        Transform(const Transform &other) = default;

        glm::mat4 getTransform() const;

        operator glm::mat4() const { return getTransform(); }

        glm::vec3 position = {0.f, 0.f, 0.f};
        glm::vec2 scale = {1.f, 1.f};
        float angle = 0.f;
    };

    class Style {
      public:
        Style() = default;
        Style(const Style &other) = default;

        glm::vec4 color = glm::vec4(1.f);
        glm::vec4 borderColor = glm::vec4(1.f);
        glm::vec4 borderSize = glm::vec4(0.f);
        glm::vec4 borderRadius = glm::vec4(0.f);
        glm::vec4 headerColor = glm::vec4(0.2f, 0.2f, 0.2f, 1.f);
    };

    // Not serialized
    struct PickingId {
        uint32_t runtimeId;
        uint32_t info;

        struct InfoFlags {
            static constexpr uint32_t unSelectable = 1 << 31;
        };

        static constexpr uint32_t invalidRuntimeId = std::numeric_limits<uint32_t>::max();

        static constexpr PickingId invalid() noexcept {
            return {invalidRuntimeId, 0};
        }

        constexpr bool operator==(const PickingId &other) const noexcept {
            return runtimeId == other.runtimeId && info == other.info;
        }

        constexpr bool isValid() const noexcept {
            return runtimeId != invalidRuntimeId;
        }

        constexpr uint64_t toUint64() const noexcept {
            return (static_cast<uint64_t>(runtimeId) << 32) | static_cast<uint64_t>(info);
        }

        constexpr operator uint64_t() const noexcept { return toUint64(); }

        static constexpr PickingId fromUint64(uint64_t value) noexcept {
            return {
                static_cast<uint32_t>(value >> 32),
                static_cast<uint32_t>(value & 0xFFFFFFFF)};
        }

        void set(uint64_t value) {
            runtimeId = static_cast<uint32_t>(value >> 32);
            info = static_cast<uint32_t>(value & 0xFFFFFFFF);
        }
    };

    enum class ConnSegOrientaion : uint8_t {
        horizontal,
        vertical
    };

    struct ConnSegment {
        glm::vec2 offset;
        ConnSegOrientaion orientation = ConnSegOrientaion::horizontal;
    };

} // namespace Bess::Canvas

namespace Bess::JsonConvert {
    void toJsonValue(const Bess::Canvas::Transform &transform, Json::Value &j);
    void fromJsonValue(const Json::Value &j, Bess::Canvas::Transform &transform);

    void toJsonValue(const Bess::Canvas::Style &style, Json::Value &j);
    void fromJsonValue(const Json::Value &j, Bess::Canvas::Style &style);

    void toJsonValue(const Bess::Canvas::ConnSegment &segment, Json::Value &j);
    void fromJsonValue(const Json::Value &j, Bess::Canvas::ConnSegment &segment);
} // namespace Bess::JsonConvert
