#pragma once

#include "bess_api.h"
#include "bess_uuid.h"
#include "json/value.h"

namespace Bess::SimEngine {
    class BESS_API SimEngineSerializer {
      public:
        SimEngineSerializer() = default;

        void serializeToPath(const std::string &path, int indent = -1);
        void serialize(Json::Value &j);
        void serializeEntity(UUID uid, Json::Value &j);

        void deserializeFromPath(const std::string &path);
        void deserialize(const Json::Value &json);
        void deserializeEntity(const Json::Value &json);

      private:
        void simulateClockedComponents();
    };
} // namespace Bess::SimEngine
