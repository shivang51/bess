#include "entt_registry_serializer.h"
#include "digital_component.h"
#include <fstream>
#include <iostream>

using namespace Bess::SimEngine;

namespace Bess {
    void EnttRegistrySerializer::serialize(const entt::registry &registry, Json::Value &j) {
        if (m_ComponentSerializers.empty()) {
            registerAll();
        }

        for (auto entity : registry.view<entt::entity>()) {
            serializeEntity(registry, entity, j["entities"].append(Json::objectValue));
        }
    }

    void EnttRegistrySerializer::serializeEntity(const entt::registry &registry, entt::entity entity, Json::Value &j) {
        for (const auto &[name, serializeCB] : m_ComponentSerializers) {
            serializeCB(registry, entity, j);
        }
    }

    void EnttRegistrySerializer::serializeToPath(const entt::registry &registry, const std::string &filename, int indent) {
        Json::Value j;
        serialize(registry, j);

        std::ofstream outFile(filename, std::ios::out);
        if (outFile.is_open()) {
            Json::StreamWriterBuilder builder;
            const std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
            writer->write(j, &outFile);
            outFile.close();
        } else {
            std::cerr << "Failed to open file for writing: " << filename << std::endl;
        }
    }

    void EnttRegistrySerializer::deserialize(entt::registry &registry, const Json::Value &json) {
        if (m_ComponentDeserializers.empty()) {
            registerAll();
        }

        if (json.isMember("entities")) {
            for (const auto &j : json["entities"]) {
                deserializeEntity(registry, j);
            }
        }
    }

    entt::entity EnttRegistrySerializer::deserializeEntity(entt::registry &registry, const Json::Value &j) {
        const auto entity = registry.create();
        for (const auto &deserializeCB : m_ComponentDeserializers | std::views::values) {
            deserializeCB(registry, entity, j);
        }
        return entity;
    }

    void EnttRegistrySerializer::deserializeFromPath(entt::registry &registry, const std::string &filename) {
        std::ifstream inFile(filename);
        if (!inFile.is_open()) {
            std::cerr << "Failed to open file for reading: " << filename << std::endl;
            return;
        }
        Json::Value data;
        inFile >> data;
        deserialize(registry, data);
    }
} // namespace Bess
