#pragma once

#include "bess_assert.h"
#include "sub_system.h"
#include "types.h"
#include <memory>
#include <typeindex>
#include <unordered_map>

namespace Bess {

    class BESS_API ISubSysContainer {
      public:
        virtual ~ISubSysContainer() = default;

        virtual void init();
        virtual void beginFrame();
        virtual void update(Bess::TimeMs dt);
        virtual void destroy();

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

      protected:
        std::unordered_map<std::type_index, std::shared_ptr<Bess::ISubSystem>>
            m_subSystems;

        bool m_initialized = false;
        bool m_destroyed = false;
    };

} // namespace Bess
