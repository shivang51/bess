#include "project_session/json_project_store.h"

#include "bess_json/bess_json.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#ifdef _WIN32
    #define NOMINMAX
    #include <windows.h>
#endif

namespace Bess::Session {
    namespace {
        constexpr std::string_view schemaName = "bess.project";

        Json::Value uuidJson(UUID id) {
            return id.toString();
        }

        UUID readUuid(const Json::Value &json,
                      std::string_view field,
                      bool allowNull = true) {
            if (json.isString()) {
                const auto text = json.asString();
                uint64_t value = 0;
                const auto [end, error] = std::from_chars(
                    text.data(), text.data() + text.size(), value);
                if (error != std::errc{} || end != text.data() + text.size()) {
                    throw std::runtime_error(std::string(field) +
                                             " contains an invalid UUID");
                }
                const UUID id{value};
                if (!allowNull && id == UUID::null) {
                    throw std::runtime_error(std::string(field) +
                                             " must not be null");
                }
                return id;
            }
            // Accept UInt64 to ease migration from the current project files.
            if (json.isUInt64() || json.isUInt()) {
                const UUID id{json.asUInt64()};
                if (!allowNull && id == UUID::null) {
                    throw std::runtime_error(std::string(field) +
                                             " must not be null");
                }
                return id;
            }
            throw std::runtime_error(std::string(field) +
                                     " must be a UUID string");
        }

        template <typename T> Json::Value vectorJson(const T &value) {
            Json::Value json;
            JsonConvert::toJsonValue(value, json);
            return json;
        }

        template <typename T>
        void readVector(const Json::Value &json, T &value) {
            JsonConvert::fromJsonValue(json, value);
        }

        Json::Value transformJson(const TransformComponent &transform) {
            Json::Value json(Json::objectValue);
            json["position"] = vectorJson(transform.position);
            json["scale"] = vectorJson(transform.scale);
            json["angle"] = transform.angle;
            return json;
        }

        TransformComponent readTransform(const Json::Value &json) {
            TransformComponent transform;
            if (!json.isObject()) {
                return transform;
            }
            if (json.isMember("position")) {
                readVector(json["position"], transform.position);
            }
            if (json.isMember("scale")) {
                readVector(json["scale"], transform.scale);
            }
            transform.angle = json.get("angle", transform.angle).asFloat();
            return transform;
        }

        Json::Value styleJson(const VisualStyleComponent &style) {
            Json::Value json(Json::objectValue);
            json["headerColor"] = vectorJson(style.headerColor);
            json["color"] = vectorJson(style.color);
            auto &schematic = json["schematic"];
            schematic["pinLabelAlignment"] =
                static_cast<int>(style.schematic.pinLabelAlignment);
            schematic["labelAlignment"] =
                static_cast<int>(style.schematic.labelAlignment);
            schematic["showPinLabels"] = style.schematic.showPinLabels;
            schematic["showName"] = style.schematic.showName;
            schematic["flipPortsX"] = style.schematic.flipPortsX;
            return json;
        }

        VisualStyleComponent readStyle(const Json::Value &json) {
            VisualStyleComponent style;
            if (!json.isObject()) {
                return style;
            }
            if (json.isMember("headerColor")) {
                readVector(json["headerColor"], style.headerColor);
            }
            if (json.isMember("color")) {
                readVector(json["color"], style.color);
            }
            if (const auto &schematic = json["schematic"];
                schematic.isObject()) {
                style.schematic.pinLabelAlignment =
                    static_cast<PinLabelAlignment>(
                        schematic
                            .get("pinLabelAlignment",
                                 static_cast<int>(
                                     style.schematic.pinLabelAlignment))
                            .asInt());
                style.schematic.labelAlignment =
                    static_cast<SchematicLabelAlignment>(
                        schematic
                            .get("labelAlignment",
                                 static_cast<int>(
                                     style.schematic.labelAlignment))
                            .asInt());
                style.schematic.showPinLabels =
                    schematic
                        .get("showPinLabels", style.schematic.showPinLabels)
                        .asBool();
                style.schematic.showName =
                    schematic.get("showName", style.schematic.showName)
                        .asBool();
                style.schematic.flipPortsX =
                    schematic.get("flipPortsX", style.schematic.flipPortsX)
                        .asBool();
            }
            return style;
        }

        Json::Value uuidArray(const std::vector<UUID> &ids) {
            Json::Value json(Json::arrayValue);
            for (const auto id : ids) {
                json.append(uuidJson(id));
            }
            return json;
        }

        std::vector<UUID> readUuidArray(const Json::Value &json,
                                        std::string_view field) {
            if (!json.isArray()) {
                throw std::runtime_error(std::string(field) +
                                         " must be an array");
            }
            std::vector<UUID> result;
            result.reserve(json.size());
            for (const auto &value : json) {
                result.push_back(readUuid(value, field));
            }
            return result;
        }

        Json::Value
        segmentArray(const std::vector<ConnectionSegment> &segments) {
            Json::Value json(Json::arrayValue);
            for (const auto &segment : segments) {
                Json::Value value(Json::objectValue);
                value["offset"] = vectorJson(segment.offset);
                value["orientation"] = static_cast<int>(segment.orientation);
                json.append(std::move(value));
            }
            return json;
        }

        std::vector<ConnectionSegment> readSegments(const Json::Value &json) {
            if (!json.isArray()) {
                throw std::runtime_error(
                    "Connection segments must be an array");
            }
            std::vector<ConnectionSegment> result;
            result.reserve(json.size());
            for (const auto &value : json) {
                if (!value.isObject() || !value.isMember("offset")) {
                    throw std::runtime_error("Connection segment is malformed");
                }
                ConnectionSegment segment;
                readVector(value["offset"], segment.offset);
                segment.orientation = static_cast<ConnectionSegmentOrientation>(
                    value
                        .get("orientation",
                             static_cast<int>(segment.orientation))
                        .asInt());
                result.push_back(segment);
            }
            return result;
        }

        std::string encodeBase64(const std::vector<uint8_t> &bytes) {
            static constexpr std::string_view alphabet =
                "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"
                "+/";
            std::string result;
            result.reserve(((bytes.size() + 2) / 3) * 4);
            for (std::size_t index = 0; index < bytes.size(); index += 3) {
                const auto remaining = bytes.size() - index;
                const uint32_t block =
                    (static_cast<uint32_t>(bytes[index]) << 16U) |
                    (remaining > 1
                         ? static_cast<uint32_t>(bytes[index + 1]) << 8U
                         : 0U) |
                    (remaining > 2 ? static_cast<uint32_t>(bytes[index + 2])
                                   : 0U);
                result.push_back(alphabet[(block >> 18U) & 0x3FU]);
                result.push_back(alphabet[(block >> 12U) & 0x3FU]);
                result.push_back(remaining > 1 ? alphabet[(block >> 6U) & 0x3FU]
                                               : '=');
                result.push_back(remaining > 2 ? alphabet[block & 0x3FU] : '=');
            }
            return result;
        }

        std::vector<uint8_t> decodeBase64(std::string_view encoded) {
            if (encoded.size() % 4 != 0) {
                throw std::runtime_error("Image base64 payload is malformed");
            }
            std::array<int16_t, 256> table;
            table.fill(-1);
            constexpr std::string_view alphabet =
                "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"
                "+/";
            for (std::size_t index = 0; index < alphabet.size(); ++index) {
                table[static_cast<uint8_t>(alphabet[index])] =
                    static_cast<int16_t>(index);
            }

            std::vector<uint8_t> result;
            result.reserve((encoded.size() / 4) * 3);
            for (std::size_t index = 0; index < encoded.size(); index += 4) {
                const char c0 = encoded[index];
                const char c1 = encoded[index + 1];
                const char c2 = encoded[index + 2];
                const char c3 = encoded[index + 3];
                if (c0 == '=' || c1 == '=' ||
                    table[static_cast<uint8_t>(c0)] < 0 ||
                    table[static_cast<uint8_t>(c1)] < 0 ||
                    (c2 != '=' && table[static_cast<uint8_t>(c2)] < 0) ||
                    (c3 != '=' && table[static_cast<uint8_t>(c3)] < 0)) {
                    throw std::runtime_error(
                        "Image base64 payload is malformed");
                }
                if (c2 == '=' && c3 != '=') {
                    throw std::runtime_error(
                        "Image base64 padding is malformed");
                }

                const uint32_t block =
                    (static_cast<uint32_t>(table[static_cast<uint8_t>(c0)])
                     << 18U) |
                    (static_cast<uint32_t>(table[static_cast<uint8_t>(c1)])
                     << 12U) |
                    (c2 == '=' ? 0U
                               : static_cast<uint32_t>(
                                     table[static_cast<uint8_t>(c2)])
                                     << 6U) |
                    (c3 == '=' ? 0U
                               : static_cast<uint32_t>(
                                     table[static_cast<uint8_t>(c3)]));
                result.push_back(static_cast<uint8_t>(block >> 16U));
                if (c2 != '=') {
                    result.push_back(static_cast<uint8_t>(block >> 8U));
                }
                if (c3 != '=') {
                    result.push_back(static_cast<uint8_t>(block));
                }
            }
            return result;
        }

        Json::Value entityJson(const EntityRecord &record) {
            Json::Value json(Json::objectValue);
            auto &identity = json["identity"];
            identity["id"] = uuidJson(record.identity.id);
            identity["name"] = record.identity.name;
            identity["icon"] = record.identity.icon;
            identity["kind"] = static_cast<int>(record.identity.kind);
            identity["showInProjectExplorer"] =
                record.identity.showInProjectExplorer;

            json["transform"] = transformJson(record.transform);
            json["style"] = styleJson(record.style);

            auto &hierarchy = json["hierarchy"];
            hierarchy["parent"] = uuidJson(record.hierarchy.parent);
            hierarchy["children"] = uuidArray(record.hierarchy.children);

            auto &interaction = json["interaction"];
            interaction["selectable"] = record.interaction.selectable;
            interaction["draggable"] = record.interaction.draggable;
            interaction["focusable"] = record.interaction.focusable;
            interaction["wantsKeyboardInput"] =
                record.interaction.wantsKeyboardInput;

            auto &components = json["components"];
            if (record.simulation) {
                auto &component = components["simulation"];
                component["netId"] = uuidJson(record.simulation->netId);
                component["definition"]["name"] =
                    record.simulation->definition.name;
                component["definition"]["pluginName"] =
                    record.simulation->definition.pluginName;
                component["schematicTransform"] =
                    transformJson(record.simulation->schematicTransform);
            }
            if (record.input) {
                components["input"] = true;
            }
            if (record.group) {
                components["group"] = true;
            }
            if (record.port) {
                auto &component = components["port"];
                component["direction"] =
                    static_cast<int>(record.port->direction);
                component["signalKind"] =
                    static_cast<int>(record.port->signalKind);
                component["index"] = record.port->index;
                component["resizeTrigger"] = record.port->resizeTrigger;
                component["schematicPosition"] =
                    vectorJson(record.port->schematicPosition);
            }
            if (record.connection) {
                auto &component = components["connection"];
                component["startPort"] = uuidJson(record.connection->startPort);
                component["endPort"] = uuidJson(record.connection->endPort);
                component["segments"] =
                    segmentArray(record.connection->segments);
                component["schematicSegments"] =
                    segmentArray(record.connection->schematicSegments);
                component["associatedJoints"] =
                    uuidArray(record.connection->associatedJoints);
                component["initialSegmentCount"] =
                    record.connection->initialSegmentCount;
                component["reconstructSegments"] =
                    record.connection->reconstructSegments;
                component["useCustomColor"] = record.connection->useCustomColor;
            }
            if (record.proxyPort) {
                auto &component = components["proxyPort"];
                component["inputPort"] = uuidJson(record.proxyPort->inputPort);
                component["outputPort"] =
                    uuidJson(record.proxyPort->outputPort);
            }
            if (record.connectionJoint) {
                auto &component = components["connectionJoint"];
                component["connection"] =
                    uuidJson(record.connectionJoint->connection);
                component["segmentIndex"] =
                    record.connectionJoint->segmentIndex;
                component["schematicSegmentIndex"] =
                    record.connectionJoint->schematicSegmentIndex;
                component["segmentOffset"] =
                    record.connectionJoint->segmentOffset;
                component["schematicOffset"] =
                    record.connectionJoint->schematicOffset;
                component["orientation"] =
                    static_cast<int>(record.connectionJoint->orientation);
            }
            if (record.module) {
                auto &component = components["module"];
                component["childScene"] = uuidJson(record.module->childScene);
                component["associatedInput"] =
                    uuidJson(record.module->associatedInput);
                component["associatedOutput"] =
                    uuidJson(record.module->associatedOutput);
            }
            if (record.image) {
                auto &component = components["image"];
                component["encoding"] = "base64";
                component["data"] = encodeBase64(record.image->rgba);
                component["width"] = record.image->width;
                component["height"] = record.image->height;
                component["maintainAspectRatio"] =
                    record.image->maintainAspectRatio;
            }
            if (record.text) {
                auto &component = components["text"];
                component["text"] = record.text->text;
                component["foregroundColor"] =
                    vectorJson(record.text->foregroundColor);
                component["fontSize"] = record.text->fontSize;
            }
            if (record.probe) {
                components["probe"]["port"] = uuidJson(record.probe->port);
            }
            if (record.monitor) {
                auto &component = components["monitor"];
                component["ports"] = uuidArray(record.monitor->ports);
                component["hiddenPorts"] =
                    uuidArray(record.monitor->hiddenPorts);
                component["timeScale"] = record.monitor->timeScale;
                component["valueScale"] = record.monitor->valueScale;
                component["followLatest"] = record.monitor->followLatest;
                component["viewEndTimeSeconds"] =
                    record.monitor->viewEndTimeSeconds;
                component["viewTimeSpanSeconds"] =
                    record.monitor->viewTimeSpanSeconds;
                component["showGrid"] = record.monitor->showGrid;
                component["showLegend"] = record.monitor->showLegend;
            }
            if (record.extensions) {
                auto &extensions = components["extensions"];
                for (const auto &[name, value] : record.extensions->values) {
                    extensions[name] = value;
                }
            }
            return json;
        }

        EntityRecord readEntity(const Json::Value &json) {
            if (!json.isObject() || !json["identity"].isObject()) {
                throw std::runtime_error(
                    "Scene entity is missing identity data");
            }

            EntityRecord record;
            const auto &identity = json["identity"];
            record.identity.id =
                readUuid(identity["id"], "entity.identity.id", false);
            record.identity.name = identity.get("name", "").asString();
            record.identity.icon = identity.get("icon", "").asString();
            const auto kind = identity.get("kind", 0).asInt();
            if (kind < static_cast<int>(EntityKind::generic) ||
                kind > static_cast<int>(EntityKind::extension)) {
                throw std::runtime_error("Entity kind is invalid");
            }
            record.identity.kind = static_cast<EntityKind>(kind);
            record.identity.showInProjectExplorer =
                identity.get("showInProjectExplorer", true).asBool();

            record.transform = readTransform(json["transform"]);
            record.style = readStyle(json["style"]);

            if (const auto &hierarchy = json["hierarchy"];
                hierarchy.isObject()) {
                record.hierarchy.parent =
                    readUuid(hierarchy["parent"], "hierarchy.parent");
                record.hierarchy.children =
                    readUuidArray(hierarchy["children"], "hierarchy.children");
            }

            if (const auto &interaction = json["interaction"];
                interaction.isObject()) {
                record.interaction.selectable =
                    interaction.get("selectable", true).asBool();
                record.interaction.draggable =
                    interaction.get("draggable", false).asBool();
                record.interaction.focusable =
                    interaction.get("focusable", false).asBool();
                record.interaction.wantsKeyboardInput =
                    interaction.get("wantsKeyboardInput", false).asBool();
            }

            const auto &components = json["components"];
            if (!components.isObject()) {
                return record;
            }
            if (const auto &component = components["simulation"];
                component.isObject()) {
                SimulationComponent simulation;
                simulation.netId =
                    readUuid(component["netId"], "simulation.netId");
                simulation.definition.name =
                    component["definition"].get("name", "").asString();
                simulation.definition.pluginName =
                    component["definition"].get("pluginName", "").asString();
                if (simulation.definition.name.empty()) {
                    throw std::runtime_error(
                        "Simulation definition name is empty");
                }
                simulation.schematicTransform =
                    readTransform(component["schematicTransform"]);
                record.simulation = std::move(simulation);
            }
            record.input = components.get("input", false).asBool();
            record.group = components.get("group", false).asBool();

            if (const auto &component = components["port"];
                component.isObject()) {
                PortComponent port;
                port.direction = static_cast<SimEngine::PortDirection>(
                    component.get("direction", 0).asInt());
                port.signalKind = static_cast<SimEngine::SignalKind>(
                    component.get("signalKind", 0).asInt());
                port.index = component.get("index", -1).asInt();
                port.resizeTrigger =
                    component.get("resizeTrigger", false).asBool();
                if (component.isMember("schematicPosition")) {
                    readVector(component["schematicPosition"],
                               port.schematicPosition);
                }
                record.port = port;
            }
            if (const auto &component = components["connection"];
                component.isObject()) {
                ConnectionComponent connection;
                connection.startPort = readUuid(
                    component["startPort"], "connection.startPort", false);
                connection.endPort =
                    readUuid(component["endPort"], "connection.endPort", false);
                connection.segments = readSegments(component["segments"]);
                connection.schematicSegments =
                    readSegments(component["schematicSegments"]);
                connection.associatedJoints =
                    readUuidArray(component["associatedJoints"],
                                  "connection.associatedJoints");
                connection.initialSegmentCount =
                    component.get("initialSegmentCount", 3).asInt();
                connection.reconstructSegments =
                    component.get("reconstructSegments", true).asBool();
                connection.useCustomColor =
                    component.get("useCustomColor", false).asBool();
                record.connection = std::move(connection);
            }
            if (const auto &component = components["proxyPort"];
                component.isObject()) {
                record.proxyPort = ProxyPortComponent{
                    .inputPort =
                        readUuid(component["inputPort"], "proxyPort.inputPort"),
                    .outputPort = readUuid(component["outputPort"],
                                           "proxyPort.outputPort"),
                };
            }
            if (const auto &component = components["connectionJoint"];
                component.isObject()) {
                ConnectionJointComponent joint;
                joint.connection = readUuid(component["connection"],
                                            "connectionJoint.connection",
                                            false);
                joint.segmentIndex = component.get("segmentIndex", -1).asInt();
                joint.schematicSegmentIndex =
                    component.get("schematicSegmentIndex", -1).asInt();
                joint.segmentOffset =
                    component.get("segmentOffset", -1.0).asFloat();
                joint.schematicOffset =
                    component.get("schematicOffset", -1.0).asFloat();
                joint.orientation = static_cast<ConnectionSegmentOrientation>(
                    component.get("orientation", 0).asInt());
                record.connectionJoint = joint;
            }
            if (const auto &component = components["module"];
                component.isObject()) {
                record.module = ModuleComponent{
                    .childScene =
                        readUuid(component["childScene"], "module.childScene"),
                    .associatedInput = readUuid(component["associatedInput"],
                                                "module.associatedInput"),
                    .associatedOutput = readUuid(component["associatedOutput"],
                                                 "module.associatedOutput"),
                };
            }
            if (const auto &component = components["image"];
                component.isObject()) {
                if (component.get("encoding", "").asString() != "base64") {
                    throw std::runtime_error("Image encoding is unsupported");
                }
                ImageComponent image;
                image.rgba = decodeBase64(component.get("data", "").asString());
                image.width = component.get("width", 0U).asUInt();
                image.height = component.get("height", 0U).asUInt();
                image.maintainAspectRatio =
                    component.get("maintainAspectRatio", true).asBool();
                const auto expectedSize = static_cast<uint64_t>(image.width) *
                                          static_cast<uint64_t>(image.height) *
                                          4U;
                if (image.rgba.size() != expectedSize) {
                    throw std::runtime_error(
                        "Image payload dimensions do not match RGBA data");
                }
                record.image = std::move(image);
            }
            if (const auto &component = components["text"];
                component.isObject()) {
                TextComponent text;
                text.text = component.get("text", "New Text").asString();
                if (component.isMember("foregroundColor")) {
                    readVector(component["foregroundColor"],
                               text.foregroundColor);
                }
                text.fontSize = component.get("fontSize", 12U).asUInt();
                record.text = std::move(text);
            }
            if (const auto &component = components["probe"];
                component.isObject()) {
                record.probe = ProbeComponent{
                    readUuid(component["port"], "probe.port", false)};
            }
            if (const auto &component = components["monitor"];
                component.isObject()) {
                MonitorComponent monitor;
                monitor.ports =
                    readUuidArray(component["ports"], "monitor.ports");
                monitor.hiddenPorts = readUuidArray(component["hiddenPorts"],
                                                    "monitor.hiddenPorts");
                monitor.timeScale = component.get("timeScale", 1.0).asFloat();
                monitor.valueScale = component.get("valueScale", 1.0).asFloat();
                monitor.followLatest =
                    component.get("followLatest", true).asBool();
                monitor.viewEndTimeSeconds =
                    component.get("viewEndTimeSeconds", 0.0).asDouble();
                monitor.viewTimeSpanSeconds =
                    component.get("viewTimeSpanSeconds", 0.0).asDouble();
                monitor.showGrid = component.get("showGrid", true).asBool();
                monitor.showLegend = component.get("showLegend", true).asBool();
                record.monitor = std::move(monitor);
            }
            if (const auto &component = components["extensions"];
                component.isObject()) {
                ExtensionComponents extensions;
                for (const auto &name : component.getMemberNames()) {
                    extensions.values.emplace(name, component[name]);
                }
                record.extensions = std::move(extensions);
            }
            return record;
        }

        Json::Value sceneJson(const SceneDocument &scene) {
            Json::Value json(Json::objectValue);
            const auto &metadata = scene.metadata();
            auto &metadataJson = json["metadata"];
            metadataJson["id"] = uuidJson(metadata.id);
            metadataJson["name"] = metadata.name;
            metadataJson["parentScene"] = uuidJson(metadata.parentScene);
            metadataJson["module"] = uuidJson(metadata.module);
            metadataJson["isRoot"] = metadata.isRoot;

            std::vector<UUID> entityIds;
            const auto view = scene.registry().view<IdentityComponent>();
            entityIds.reserve(scene.size());
            for (const auto entity : view) {
                entityIds.push_back(view.get<IdentityComponent>(entity).id);
            }
            std::sort(entityIds.begin(), entityIds.end(), [](UUID a, UUID b) {
                return static_cast<uint64_t>(a) < static_cast<uint64_t>(b);
            });

            auto &entities = json["entities"];
            entities = Json::arrayValue;
            for (const auto id : entityIds) {
                auto record = scene.captureEntity(id);
                if (!record) {
                    throw std::runtime_error(
                        "Could not capture scene entity: " +
                        record.error().message);
                }
                // Selection, focus, picking IDs, and probe samples are runtime
                // state and are intentionally not encoded.
                record->selected = false;
                record->focused = false;
                record->probeRuntime.reset();
                entities.append(entityJson(*record));
            }
            return json;
        }

        SceneMetadata readSceneMetadata(const Json::Value &json) {
            if (!json.isObject()) {
                throw std::runtime_error("Scene metadata is missing");
            }
            SceneMetadata metadata;
            metadata.id = readUuid(json["id"], "scene.id", false);
            metadata.name = json.get("name", "Scene").asString();
            metadata.parentScene =
                readUuid(json["parentScene"], "scene.parentScene");
            metadata.module = readUuid(json["module"], "scene.module");
            metadata.isRoot = json.get("isRoot", false).asBool();
            return metadata;
        }

        Status replaceFile(const std::filesystem::path &temporary,
                           const std::filesystem::path &destination) {
#ifdef _WIN32
            if (!MoveFileExW(temporary.c_str(),
                             destination.c_str(),
                             MOVEFILE_REPLACE_EXISTING |
                                 MOVEFILE_WRITE_THROUGH)) {
                return fail(Error{
                    ErrorCode::persistenceFailure,
                    "Could not atomically replace project file (Windows "
                    "error " +
                        std::to_string(GetLastError()) + ")",
                });
            }
#else
            std::error_code error;
            std::filesystem::rename(temporary, destination, error);
            if (error) {
                return fail(Error{
                    ErrorCode::persistenceFailure,
                    "Could not atomically replace project file: " +
                        error.message(),
                });
            }
#endif
            return {};
        }
    } // namespace

    Result<Json::Value>
    JsonProjectStore::encode(const ProjectDocument &document) const {
        if (auto status = document.validate(); !status) {
            return fail(Error{
                ErrorCode::persistenceFailure,
                "Refusing to encode an invalid project: " +
                    status.error().message,
            });
        }

        try {
            Json::Value json(Json::objectValue);
            json["schema"] = std::string(schemaName);
            json["formatVersion"] = document.metadata().formatVersion;
            json["project"]["id"] = uuidJson(document.metadata().id);
            json["project"]["name"] = document.metadata().name;
            json["rootScene"] = uuidJson(document.rootSceneId());
            json["activeScene"] = uuidJson(document.activeSceneId());
            json["scenes"] = Json::arrayValue;
            for (const auto sceneId : document.sceneOrder()) {
                json["scenes"].append(sceneJson(*document.scene(sceneId)));
            }
            return json;
        } catch (const std::exception &exception) {
            return fail(Error{
                ErrorCode::persistenceFailure,
                "Could not encode project: " + std::string(exception.what()),
            });
        }
    }

    Result<std::unique_ptr<ProjectDocument>>
    JsonProjectStore::decode(const Json::Value &json) const {
        try {
            if (!json.isObject() ||
                json.get("schema", "").asString() != schemaName) {
                return fail(Error{
                    ErrorCode::persistenceFailure,
                    "Unsupported project schema; legacy projects require an "
                    "explicit migration importer",
                });
            }
            const auto version = json.get("formatVersion", 0U).asUInt();
            if (version == 0 ||
                version > ProjectMetadata::currentFormatVersion) {
                return fail(Error{
                    ErrorCode::persistenceFailure,
                    "Unsupported project format version " +
                        std::to_string(version),
                });
            }
            if (!json["project"].isObject() || !json["scenes"].isArray() ||
                json["scenes"].empty()) {
                throw std::runtime_error(
                    "Project metadata or scenes are missing");
            }

            ProjectMetadata metadata;
            metadata.id = readUuid(json["project"]["id"], "project.id", false);
            metadata.name =
                json["project"].get("name", "Untitled Project").asString();
            metadata.formatVersion = version;
            auto document =
                std::make_unique<ProjectDocument>(std::move(metadata));
            document->clear();

            for (const auto &sceneValue : json["scenes"]) {
                const auto sceneMetadata =
                    readSceneMetadata(sceneValue["metadata"]);
                auto created = document->createScene(
                    sceneMetadata.name, false, sceneMetadata.id);
                if (!created) {
                    throw std::runtime_error(created.error().message);
                }
                auto *scene = *created;
                scene->metadata() = sceneMetadata;

                const auto &entities = sceneValue["entities"];
                if (!entities.isArray()) {
                    throw std::runtime_error("Scene entities must be an array");
                }
                SceneMutation mutation;
                mutation.removed.reserve(entities.size());
                for (const auto &entity : entities) {
                    mutation.removed.push_back(readEntity(entity));
                }
                if (auto status = scene->restoreMutation(mutation); !status) {
                    throw std::runtime_error(status.error().message);
                }
            }

            const auto root = readUuid(json["rootScene"], "rootScene", false);
            const auto active =
                readUuid(json["activeScene"], "activeScene", false);
            if (auto status = document->setRootScene(root); !status) {
                throw std::runtime_error(status.error().message);
            }
            if (auto status = document->setActiveScene(active); !status) {
                throw std::runtime_error(status.error().message);
            }
            if (auto status = document->rebuildIndices(); !status) {
                throw std::runtime_error(status.error().message);
            }
            return document;
        } catch (const std::exception &exception) {
            return fail(Error{
                ErrorCode::persistenceFailure,
                "Could not decode project: " + std::string(exception.what()),
            });
        }
    }

    Result<std::unique_ptr<ProjectDocument>>
    JsonProjectStore::load(const std::filesystem::path &path) {
        if (path.empty()) {
            return fail(Error::invalidArgument("Project path is empty"));
        }
        std::ifstream input(path, std::ios::binary);
        if (!input.is_open()) {
            return fail(Error{
                ErrorCode::persistenceFailure,
                "Could not open project file '" + path.string() + "'",
            });
        }

        Json::Value json;
        Json::CharReaderBuilder builder;
        std::string errors;
        if (!Json::parseFromStream(builder, input, &json, &errors)) {
            return fail(Error{
                ErrorCode::persistenceFailure,
                "Could not parse project JSON: " + errors,
            });
        }
        return decode(json);
    }

    Status JsonProjectStore::save(const std::filesystem::path &path,
                                  const ProjectDocument &document) {
        if (path.empty()) {
            return fail(Error::invalidArgument("Project path is empty"));
        }
        auto json = encode(document);
        if (!json) {
            return fail(std::move(json.error()));
        }

        const auto parent = path.has_parent_path() ? path.parent_path()
                                                   : std::filesystem::path{"."};
        std::error_code filesystemError;
        if (!std::filesystem::exists(parent, filesystemError) ||
            filesystemError) {
            return fail(Error{
                ErrorCode::persistenceFailure,
                "Project directory does not exist: " + parent.string(),
            });
        }

        auto temporary = path;
        temporary += ".bess-tmp-" + UUID{}.toString();
        {
            std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
            if (!output.is_open()) {
                return fail(Error{
                    ErrorCode::persistenceFailure,
                    "Could not open temporary project file for writing",
                });
            }
            Json::StreamWriterBuilder builder;
            builder["commentStyle"] = "None";
            builder["indentation"] = "  ";
            const std::unique_ptr<Json::StreamWriter> writer(
                builder.newStreamWriter());
            if (writer->write(*json, &output) != 0) {
                output.close();
                std::filesystem::remove(temporary, filesystemError);
                return fail(Error{
                    ErrorCode::persistenceFailure,
                    "Could not write project JSON",
                });
            }
            output.flush();
            if (!output.good()) {
                output.close();
                std::filesystem::remove(temporary, filesystemError);
                return fail(Error{
                    ErrorCode::persistenceFailure,
                    "Could not flush project JSON to disk",
                });
            }
        }

        auto status = replaceFile(temporary, path);
        if (!status) {
            std::filesystem::remove(temporary, filesystemError);
        }
        return status;
    }
} // namespace Bess::Session
