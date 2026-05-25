#pragma once

#include "common/bess_api.h"
#include "common/bess_assert.h"
#include "sub_system.h"
#include <memory>
#include <typeindex>
#include <unordered_map>

namespace Bess {
    class BESS_API GAppContext {
      public:
        static GAppContext &getInstance() {
            static GAppContext instance;
            return instance;
        }

        void init();

        void update(TimeMs dt);

        void destroy();

        template <typename T, typename... Args>
            requires std::derived_from<T, ISubSystem>
        std::shared_ptr<T> addSubSystem(Args &&...args) {
            auto subsystem = std::make_shared<T>(std::forward<Args>(args)...);
            m_subSystems[typeid(T)] = subsystem;
            return subsystem;
        }

        template <typename T> std::shared_ptr<T> getSubSystem() {
            auto it = m_subSystems.find(typeid(T));
            if (it != m_subSystems.end()) {
                return std::static_pointer_cast<T>(it->second);
            }
            BESS_ASSERT(false, "SubSystem of type {} not found in GAppContext",
                        typeid(T).name());
            return nullptr;
        }

        template <typename T> void removeSubSystem() {
            m_subSystems.erase(typeid(T));
        }

        GAppContext(const GAppContext &) = delete;
        GAppContext &operator=(const GAppContext &) = delete;

      private:
        bool m_initialized = false;
        bool m_destroyed = false;

        std::unordered_map<std::type_index, std::shared_ptr<ISubSystem>>
            m_subSystems;

        GAppContext() = default;
        ~GAppContext() = default;
    };
} // namespace Bess
