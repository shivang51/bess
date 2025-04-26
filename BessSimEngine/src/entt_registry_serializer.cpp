#include "entt_registry_serializer.h"
#include "entt_components.h"
#include <fstream>
#include <iostream>

using namespace Bess::SimEngine;

namespace Bess {
    nlohmann::json EnttRegistrySerializer::serialize(const entt::registry &registry) {
        nlohmann::json jsonData;

        for (auto entity : registry.view<entt::entity>()) {
            jsonData["entities"].push_back(serializeEntity(const_cast<entt::registry &>(registry), entity));
        };

        return jsonData;
    }

    void EnttRegistrySerializer::serializeToPath(const entt::registry &registry, const std::string &filename, int indent) {
        auto serializedData = serialize(registry);

        std::ofstream outFile(filename, std::ios::out);
        if (outFile.is_open()) {
            outFile << serializedData.dump(indent);
            outFile.close();
        } else {
            std::cerr << "Failed to open file for writing: " << filename << std::endl;
        }
    }

    void EnttRegistrySerializer::deserialize(entt::registry &registry, const nlohmann::json &json) {
        for (const auto &j : json["entities"]) {
            deserializeEntity(registry, j);
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
