#pragma once

#include "entt_registry_serializer.h"

namespace Bess {
    class SceneSerializer : EnttRegistrySerializer {
      public:
        SceneSerializer() = default;

        void serializeToPath(const std::string &path, int indent = -1);
        void serialize(nlohmann::json &j);

        void deserializeFromPath(const std::string &path);
        void deserialize(const nlohmann::json &json);

        void registerAll() override;

      private:
        float m_maxZ = 0;
    };
} // namespace Bess
