#pragma once
#include "bess_api.h"
#include "json/json.h"
#include "logger.h"
#include <entt/entt.hpp>

// Forward declare the namespace that your helpers actually live in.
namespace Bess::JsonConvert {
    // Forward declare the template functions that will be called.
    template <typename T>
    void toJsonValue(const T &, Json::Value &);
    template <typename T>
    void fromJsonValue(const Json::Value &, T &);
}


namespace Bess {
    class BESS_API EnttRegistrySerializer {
      public:
        void serializeToPath(const entt::registry &registry, const std::string &filename, int indent = -1);
        void serialize(const entt::registry &registry, Json::Value &j);

        void deserializeFromPath(entt::registry &registry, const std::string &filename);
        void deserialize(entt::registry &registry, const Json::Value &json);

        /// @brief Implement this and use registerComponent to register all components you want to serialize.
        virtual void registerAll() = 0;

    protected:
        template<typename T>
        void registerComponent(const std::string &name) {
          m_ComponentSerializers[name] = [name](const entt::registry& registry, entt::entity entity, Json::Value& j) {
            if (auto* component = registry.try_get<T>(entity)) {
                Bess::JsonConvert::toJsonValue(*component, j[name]);
            }
          };

          m_ComponentDeserializers[name] = [name](entt::registry& registry, entt::entity entity, const Json::Value& j) {
            if (j.isMember(name)) {
                  T component{};
                  Bess::JsonConvert::fromJsonValue(j[name], component);
                  registry.emplace<T>(entity, component);
            }
          };
        }

        void serializeEntity(const entt::registry &registry, entt::entity entity, Json::Value &j);
        void deserializeEntity(entt::registry &registry, const Json::Value &j);

    private:
        using SerializeCB = std::function<void(const entt::registry&, entt::entity, Json::Value&)>;
        using DeserializeCB = std::function<void(entt::registry&, entt::entity, const Json::Value&)>;

        // using map instead of unordered_map because unordered_map was causing heap corruption when map had to resize.
        std::map<std::string, SerializeCB> m_ComponentSerializers;
        std::map<std::string, DeserializeCB> m_ComponentDeserializers;
    };

} // namespace Bess
