#pragma once

#include "common/bess_api.h"
#include "common/bess_uuid.h"
#include "common/types.h"
#include "json/value.h"

#include "glm.hpp"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Bess::Session {
    enum class EntityKind : uint8_t {
        generic,
        simulation,
        input,
        port,
        connection,
        connectionJoint,
        group,
        module,
        image,
        text,
        probe,
        monitor,
        extension,
    };

    struct BESS_API IdentityComponent {
        UUID id = UUID::null;
        std::string name;
        std::string icon;
        EntityKind kind = EntityKind::generic;
        bool showInProjectExplorer = true;
    };

    struct BESS_API TransformComponent {
        glm::vec3 position{0.0F};
        glm::vec2 scale{100.0F, 100.0F};
        float angle = 0.0F;
    };

    enum class PinLabelAlignment : uint8_t {
        adjacent,
        topCenter,
        bottomCenter,
    };

    enum class SchematicLabelAlignment : uint8_t {
        center,
        topCenter,
        bottomCenter,
    };

    struct BESS_API SchematicStyle {
        PinLabelAlignment pinLabelAlignment = PinLabelAlignment::adjacent;
        SchematicLabelAlignment labelAlignment =
            SchematicLabelAlignment::center;
        bool showPinLabels = true;
        bool showName = true;
        bool flipPortsX = false;
    };

    struct BESS_API VisualStyleComponent {
        SchematicStyle schematic;
        glm::vec4 headerColor{0.11F, 0.15F, 0.21F, 1.0F};
        glm::vec4 color{1.0F};
    };

    struct BESS_API HierarchyComponent {
        UUID parent = UUID::null;
        std::vector<UUID> children;
    };

    struct BESS_API InteractionComponent {
        bool selectable = true;
        bool draggable = false;
        bool focusable = false;
        bool wantsKeyboardInput = false;
    };

    struct BESS_API PickingComponent {
        uint32_t runtimeId = PickingId::invalidRuntimeId;
    };

    // Empty components are intentionally used as transient EnTT tags.
    struct BESS_API SelectedTag {};
    struct BESS_API FocusedTag {};

    struct BESS_API SimulationComponent {
        UUID simulationId = UUID::null;
        UUID netId = UUID::null;
        SimEngine::CompDefRef definition;
        TransformComponent schematicTransform;
    };

    struct BESS_API InputComponent {};
    struct BESS_API GroupComponent {};

    struct BESS_API PortComponent {
        SimEngine::PortDirection direction = SimEngine::PortDirection::none;
        SimEngine::SignalKind signalKind = SimEngine::SignalKind::none;
        int index = -1;
        bool resizeTrigger = false;
        glm::vec3 schematicPosition{0.0F};
    };

    enum class ConnectionSegmentOrientation : uint8_t {
        horizontal,
        vertical,
    };

    struct BESS_API ConnectionSegment {
        glm::vec2 offset{0.0F};
        ConnectionSegmentOrientation orientation =
            ConnectionSegmentOrientation::horizontal;
    };

    struct BESS_API ConnectionComponent {
        UUID startPort = UUID::null;
        UUID endPort = UUID::null;
        std::vector<ConnectionSegment> segments;
        std::vector<ConnectionSegment> schematicSegments;
        std::vector<UUID> associatedJoints;
        int initialSegmentCount = 3;
        bool reconstructSegments = true;
        bool useCustomColor = false;
    };

    struct BESS_API ProxyPortComponent {
        UUID inputPort = UUID::null;
        UUID outputPort = UUID::null;
    };

    struct BESS_API ConnectionJointComponent {
        UUID connection = UUID::null;
        int segmentIndex = -1;
        int schematicSegmentIndex = -1;
        float segmentOffset = -1.0F;
        float schematicOffset = -1.0F;
        ConnectionSegmentOrientation orientation =
            ConnectionSegmentOrientation::horizontal;
    };

    struct BESS_API ModuleComponent {
        UUID childScene = UUID::null;
        UUID associatedInput = UUID::null;
        UUID associatedOutput = UUID::null;
    };

    struct BESS_API ImageComponent {
        std::vector<uint8_t> rgba;
        uint32_t width = 0;
        uint32_t height = 0;
        bool maintainAspectRatio = true;
    };

    struct BESS_API TextComponent {
        std::string text = "New Text";
        glm::vec4 foregroundColor{1.0F};
        uint32_t fontSize = 12;
    };

    struct BESS_API ProbeSample {
        TimeNs time{0};
        SimEngine::PortState state;
    };

    struct BESS_API ProbeComponent {
        UUID port = UUID::null;
    };

    // Simulation samples are undoable runtime state, but project stores must
    // not persist them. They can grow without bound during a long run.
    struct BESS_API ProbeRuntimeComponent {
        std::vector<ProbeSample> samples;
    };

    struct BESS_API MonitorComponent {
        std::vector<UUID> ports;
        std::vector<UUID> hiddenPorts;
        float timeScale = 1.0F;
        float valueScale = 1.0F;
        bool followLatest = true;
        double viewEndTimeSeconds = 0.0;
        double viewTimeSpanSeconds = 0.0;
        bool showGrid = true;
        bool showLegend = true;
    };

    // Plugins can persist data without forcing the project model to depend on
    // plugin-defined C++ types. Runtime systems may materialize faster native
    // components from this payload when a plugin is loaded.
    struct BESS_API ExtensionComponents {
        std::unordered_map<std::string, Json::Value> values;
    };
} // namespace Bess::Session
