#pragma once

#include "entt_registry_serializer.h"

namespace Bess::SimEngine {
    class SimEngineSerializer : EnttRegistrySerializer {
      public:
        SimEngineSerializer() = default;

        void serializeToPath(const std::string &path, int indent = -1);
        std::string serialize(int indent = -1);

        void deserializeFromPath(const std::string &path);
        void deserialize(const std::string &json);

        nlohmann::json serializeEntity(entt::registry &registry, entt::entity entity) override;
        void deserializeEntity(entt::registry &registry, const nlohmann::json &j) override;
    };
} // namespace Bess::SimEngine
