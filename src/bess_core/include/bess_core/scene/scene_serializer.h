#pragma once

#include "common/bess_uuid.h"
#include "json/json.h"
#include <string>

namespace Bess {
    namespace Canvas {
        class Scene;
    }

    class SceneSerializer {
      public:
        SceneSerializer() = default;

        void serializeToPath(const std::string &path, int indent = -1) const;
        void serialize(Json::Value &json,
                       const std::shared_ptr<Canvas::Scene> &scene) const;
        void serializeEntity(UUID uid, Json::Value &j) const;

        void deserializeFromPath(const std::string &path);
        void deserialize(Json::Value &json,
                         const std::shared_ptr<Canvas::Scene> &scene);
        void deserializeEntity(const Json::Value &json);

      private:
        float m_maxZ = 0;
    };
} // namespace Bess
