#include "entt_registry_serializer.h"
#include "entt_components.h"
#include <fstream>
#include <iostream>

using namespace Bess::SimEngine;

namespace Bess {
    void EnttRegistrySerializer::serialize(const entt::registry &registry, nlohmann::json &j) {
        if(m_ComponentSerializers.empty()) {
            registerAll();
        }

        for (auto entity : registry.view<entt::entity>()) {
            serializeEntity(registry, entity, j["entities"].emplace_back());
        }
    }

    void EnttRegistrySerializer::serializeEntity(const entt::registry &registry, entt::entity entity, nlohmann::json &j) {
        for (const auto &[name, serializeCB] : m_ComponentSerializers) {
            serializeCB(registry, entity, j);
        }
    }

    void EnttRegistrySerializer::serializeToPath(const entt::registry &registry, const std::string &filename, int indent) {
        nlohmann::json j;
        serialize(registry, j);

        std::ofstream outFile(filename, std::ios::out);
        if (outFile.is_open()) {
            outFile << j.dump(indent);
            outFile.close();
        } else {
            std::cerr << "Failed to open file for writing: " << filename << std::endl;
        }
    }

    void EnttRegistrySerializer::deserialize(entt::registry &registry, const nlohmann::json &json) {
        if(m_ComponentDeserializers.empty()) {
            registerAll();
        }

        if(json.contains("entities")) {
            for (const auto &j : json["entities"]) {
                deserializeEntity(registry, j);
            }
        }
    }

    void EnttRegistrySerializer::deserializeEntity(entt::registry &registry, const nlohmann::json &j) {
        for (const auto &[name, deserializeCB] : m_ComponentDeserializers) {
            deserializeCB(registry, j);
        }
    }

    void EnttRegistrySerializer::deserializeFromPath(entt::registry &registry, const std::string &filename) {
        std::ifstream inFile(filename);
        if (!inFile.is_open()) {
            std::cerr << "Failed to open file for reading: " << filename << std::endl;
            return;
        }
        nlohmann::json data;
        inFile >> data;
        deserialize(registry, data);
    }
} // namespace Bess
