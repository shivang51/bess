#pragma once
#include "json.hpp"
#include <entt/entt.hpp>

namespace Bess {
    class EnttRegistrySerializer {
      public:
        void serializeToPath(const entt::registry &registry, const std::string &filename, int indent = -1);
        std::string serialize(const entt::registry &registry, int indent = -1);

        void deserializeFromPath(entt::registry &registry, const std::string &filename);
        void deserialize(entt::registry &registry, const std::string &json);

        virtual nlohmann::json serializeEntity(entt::registry &registry, entt::entity entity) = 0;
        virtual void deserializeEntity(entt::registry &registry, const nlohmann::json &j) = 0;
    };

} // namespace Bess
