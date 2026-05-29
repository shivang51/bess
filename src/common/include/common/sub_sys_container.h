#pragma once

#include "bess_assert.h"
#include "common/logger.h"
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
        virtual void endFrame();
        virtual void draw();
        virtual void preDraw();
        virtual void postDraw();
        virtual void preUpdate();
        virtual void update(Bess::TimeMs dt);
        virtual void destroy();

        template <typename T> bool hasSubSystem() const {
            return m_subSystems.contains(typeid(T));
        }

        template <typename T, typename... Args>
            requires std::derived_from<T, ISubSystem>
        std::shared_ptr<T> addSubSystem(Args &&...args) {
            if (hasSubSystem<T>()) {
                BESS_WARN("SubSystem of type {} already exists, skipping add",
                          typeid(T).name());
                return getSubSystem<T>();
            }
            auto subsystem = std::make_shared<T>(std::forward<Args>(args)...);
            m_subSystems[typeid(T)] = subsystem;
            m_subSystemsInOrder.push_back(subsystem);
            return subsystem;
        }

        template <typename T>
            requires std::derived_from<T, ISubSystem>
        std::shared_ptr<T> getSubSystem() const {
            auto it = m_subSystems.find(typeid(T));
            if (it != m_subSystems.end()) {
                return std::static_pointer_cast<T>(it->second);
            }
            BESS_ASSERT(false, "SubSystem of type {} not found",
                        typeid(T).name());
            return nullptr;
        }

        template <typename T>
            requires std::derived_from<T, ISubSystem>
        void removeSubSystem() {
            m_subSystemsInOrder.erase(
                std::remove_if(
                    m_subSystemsInOrder.begin(), m_subSystemsInOrder.end(),
                    [](const std::shared_ptr<ISubSystem> &subsystem) {
                        return std::dynamic_pointer_cast<T>(subsystem) !=
                               nullptr;
                    }),
                m_subSystemsInOrder.end());
            m_subSystems.erase(typeid(T));
        }

      protected:
        std::unordered_map<std::type_index, std::shared_ptr<Bess::ISubSystem>>
            m_subSystems;
        std::vector<std::shared_ptr<Bess::ISubSystem>> m_subSystemsInOrder;

        bool m_initialized = false;
        bool m_destroyed = false;
    };

} // namespace Bess
