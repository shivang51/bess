#pragma once

#include "entt_registry_serializer.h"

namespace Bess {
    class SceneSerializer : EnttRegistrySerializer {
      public:
        SceneSerializer() = default;

        void serializeToPath(const std::string &path, int indent = -1);
        nlohmann::json serialize();

        void deserializeFromPath(const std::string &path);
        void deserialize(const nlohmann::json &json);

        nlohmann::json serializeEntity(entt::registry &registry, entt::entity entity) override;
        void deserializeEntity(entt::registry &registry, const nlohmann::json &j) override;
    };
} // namespace Bess
