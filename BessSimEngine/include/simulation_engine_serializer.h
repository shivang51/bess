#pragma once

#include "bess_api.h"
#include "entt_registry_serializer.h"

namespace Bess::SimEngine {
    class BESS_API SimEngineSerializer : EnttRegistrySerializer {
      public:
        SimEngineSerializer() = default;

        void serializeToPath(const std::string &path, int indent = -1);
        void serialize(nlohmann::json &j);

        void deserializeFromPath(const std::string &path);
        void deserialize(const nlohmann::json &json);

        void registerAll() override;

      private:
        void simulateClockedComponents();
    };
} // namespace Bess::SimEngine
