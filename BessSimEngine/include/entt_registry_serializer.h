#pragma once
#include "json.hpp"
#include <entt/entt.hpp>

namespace Bess {
    class EnttRegistrySerializer {
      public:
        void serializeToPath(const entt::registry &registry, const std::string &filename, int indent = -1);
        nlohmann::json serialize(const entt::registry &registry);

        void deserializeFromPath(entt::registry &registry, const std::string &filename);
        void deserialize(entt::registry &registry, const nlohmann::json &json);

        virtual nlohmann::json serializeEntity(entt::registry &registry, entt::entity entity) = 0;
        virtual void deserializeEntity(entt::registry &registry, const nlohmann::json &j) = 0;
    };

} // namespace Bess
