#pragma once

#include "bess_uuid.h"
#include "json/json.h"
#include <string>

namespace Bess {
    class SceneSerializer {
      public:
        SceneSerializer() = default;

        void serializeToPath(const std::string &path, int indent = -1);
        void serialize(Json::Value &j);
        void serializeEntity(UUID uid, Json::Value &j);

        void deserializeFromPath(const std::string &path);
        void deserialize(const Json::Value &json);
        void deserializeEntity(const Json::Value &json);

      private:
        float m_maxZ = 0;
    };
} // namespace Bess
