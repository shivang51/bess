#pragma once

#include "bess_api.h"
#include "comp_json_converters.h"
#include "entt_registry_serializer.h"

namespace Bess::SimEngine {
    class BESS_API SimEngineSerializer : EnttRegistrySerializer {
      public:
        SimEngineSerializer() = default;

        void serializeToPath(const std::string &path, int indent = -1);
        void serialize(Json::Value &j);

        void deserializeFromPath(const std::string &path);
        void deserialize(const Json::Value &json);

        void registerAll() override;

      private:
        void simulateClockedComponents();
    };
} // namespace Bess::SimEngine
