#pragma once

#include "bess_api.h"
#include "entt_registry_serializer.h"

namespace Bess::SimEngine {
    class BESS_API SimEngineSerializer : EnttRegistrySerializer {
      public:
        SimEngineSerializer() = default;

        void serializeToPath(const std::string &path, int indent = -1);
        nlohmann::json serialize();

        void deserializeFromPath(const std::string &path);
        void deserialize(const nlohmann::json &json);

        nlohmann::json serializeEntity(entt::registry &registry, entt::entity entity) override;
        void deserializeEntity(entt::registry &registry, const nlohmann::json &j) override;

      private:
        void simulateClockedComponents();
    };
} // namespace Bess::SimEngine
