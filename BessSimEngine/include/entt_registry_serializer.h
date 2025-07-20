#pragma once
#include "bess_api.h"
#include "json.hpp"
#include <entt/entt.hpp>
#include "logger.h"

namespace Bess {
    class BESS_API EnttRegistrySerializer {
      public:
        void serializeToPath(const entt::registry &registry, const std::string &filename, int indent = -1);
        void serialize(const entt::registry &registry, nlohmann::json &j);

        void deserializeFromPath(entt::registry &registry, const std::string &filename);
        void deserialize(entt::registry &registry, const nlohmann::json &json);

        /// @brief Implement this and use registerComponent to register all components you want to serialize.
        virtual void registerAll() = 0;

      protected:
        template<typename T>
        void registerComponent(const std::string &name) {
          m_ComponentSerializers[name] = [name](const entt::registry& registry, entt::entity entity, nlohmann::json& j) {
            if (auto* component = registry.try_get<T>(entity)) {
                j[name] = *component;
            }
          };

          m_ComponentDeserializers[name] = [name](entt::registry& registry, entt::entity entity, const nlohmann::json& j) {
            if (j.contains(name)) {
                auto component = j.at(name).get<T>();
                registry.emplace<T>(entity, component);
            }
          };
        }

      private:
        using SerializeCB = std::function<void(const entt::registry&, entt::entity, nlohmann::json&)>;
        using DeserializeCB = std::function<void(entt::registry&, entt::entity, const nlohmann::json&)>;

        // using map instead of unordered_map because unordered_map was causing heap corruption when map had to resize.
        std::map<std::string, SerializeCB> m_ComponentSerializers;
        std::map<std::string, DeserializeCB> m_ComponentDeserializers;

        void serializeEntity(const entt::registry &registry, entt::entity entity, nlohmann::json &j);
        void deserializeEntity(entt::registry &registry, const nlohmann::json &j);
    };

} // namespace Bess
