#include "entt_registry_serializer.h"
#include "entt_components.h"
#include <fstream>
#include <iostream>

using namespace Bess::SimEngine;

namespace Bess {
    std::string EnttRegistrySerializer::serialize(const entt::registry &registry, int indent) {
        nlohmann::json jsonData;

        for (auto entity : registry.view<entt::entity>()) {
            jsonData["entities"].push_back(serializeEntity(const_cast<entt::registry &>(registry), entity));
        };

        return jsonData.dump(indent);
    }

    void EnttRegistrySerializer::serializeToPath(const entt::registry &registry, const std::string &filename, int indent) {
        auto serializedData = serialize(registry, indent);

        std::ofstream outFile(filename, std::ios::out);
        if (outFile.is_open()) {
            outFile << serializedData;
            outFile.close();
        } else {
            std::cerr << "Failed to open file for writing: " << filename << std::endl;
        }
    }

    void EnttRegistrySerializer::deserialize(entt::registry &registry, const std::string &json) {
        nlohmann::json jsonData = nlohmann::json::parse(json);
        for (const auto &j : jsonData["entities"]) {
            deserializeEntity(registry, j);
        }
    }

    void EnttRegistrySerializer::deserializeFromPath(entt::registry &registry, const std::string &filename) {
        std::ifstream inFile(filename);
        if (!inFile.is_open()) {
            std::cerr << "Failed to open file for reading: " << filename << std::endl;
            return;
        }
        std::stringstream buffer;
        buffer << inFile.rdbuf();
        deserialize(registry, buffer.str());
    }
} // namespace Bess
