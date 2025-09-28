#pragma once

#include "scene/components/components.h"
#include "scene/components/non_sim_comp.h"
#include "entt_registry_serializer.h"


namespace Bess {
    class SceneSerializer final : public EnttRegistrySerializer {
      public:
        SceneSerializer();

        void serializeToPath(const std::string &path, int indent = -1);
        void serialize(Json::Value &j);
        void serializeEntity(UUID uid, Json::Value &j);

        void deserializeFromPath(const std::string &path);
        void deserialize(const Json::Value &json);
        void deserializeEntity(const Json::Value &json);

        void registerAll() override;

      private:
        float m_maxZ = 0;
    };
} // namespace Bess
