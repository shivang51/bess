#pragma once
#include "bess_api.h"
#include "common/class_helpers.h"
#include "common/logger.h"
#include <common/bess_uuid.h>
#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace Bess::SimEngine::Drivers {

    class BESS_API SimFnDataBase {
      public:
        virtual ~SimFnDataBase() = default;
    };

    class BESS_API ComponentDef {
      public:
        typedef std::function<bool(const SimFnDataBase &)> SimFn;

        ComponentDef() = default;
        virtual ~ComponentDef() = default;

        MAKE_GETTER_SETTER(std::string, TypeName, m_typeName)
        MAKE_GETTER_SETTER(std::string, Name, m_name)
        MAKE_GETTER_SETTER(std::string, GroupName, m_groupName)
        MAKE_VGETTER_VSETTER(SimFn, SimFn, m_simFn)

        virtual Json::Value toJson() const {
            Json::Value json;
            json["groupName"] = m_groupName;
            json["typeName"] = m_typeName;
            json["name"] = m_name;
            return json;
        }

        static void fromJson(const std::shared_ptr<ComponentDef> &compDef,
                             const Json::Value &json) {}

        virtual std::shared_ptr<ComponentDef> clone() const {
            return std::make_shared<ComponentDef>(*this);
        }

      protected:
        std::string m_typeName;
        std::string m_name;
        std::string m_groupName;
        SimFn m_simFn = nullptr;
    };

    class BESS_API SimComponent {
      public:
        SimComponent() = default;
        virtual ~SimComponent() = default;

        MAKE_GETTER_SETTER(UUID, Uuid, m_uuid)
        MAKE_GETTER_SETTER(std::string, Name, m_name)
        MAKE_GETTER_SETTER(std::shared_ptr<ComponentDef>,
                           Definition,
                           m_def)

        virtual Json::Value toJson() const {
            Json::Value json;
            JsonConvert::toJsonValue(m_uuid, json["uuid"]);
            json["name"] = m_name;
            json["def"] = m_def ? m_def->toJson() : Json::Value();
            return json;
        }
        static void fromJson(const std::shared_ptr<SimComponent> &comp,
                             const Json::Value &json);

        virtual bool simulate(const SimFnDataBase &data) {
            if (!m_def) {
                BESS_WARN("(SimComponent.simulate) No definition for component with UUID {}",
                          (uint64_t)m_uuid);
                return false;
            }

            auto simFn = m_def->getSimFn();

            if (simFn) {
                return simFn(data);
            }

            BESS_WARN("(SimComponent.simulate) No sim function for component definition of component with UUID {}",
                      (uint64_t)m_uuid);
            return false;
        }

      protected:
        UUID m_uuid; // will auto gen id for each instance
        std::string m_name;
        std::shared_ptr<ComponentDef> m_def = nullptr;
    };

    enum class SimDriverState : uint8_t {
        uninitialized,
        destroyed,
        stopped,
        running,
        paused
    };

    class BESS_API SimDriver {
      public:
        SimDriver() = default;
        virtual ~SimDriver() = default;

        virtual void onInit() {};

        // will be ran in seperate thread
        virtual void run() = 0;

      protected:
        virtual void onStop() {};

        virtual void onReset() {};

        virtual void onPause() {};

        virtual void onResume() {};

        virtual void onStep() {};

        virtual void onDestroy() {};

      public:
        typedef std::shared_ptr<SimComponent> SimComponentPtr;
        typedef std::unordered_map<UUID, SimComponentPtr> ComponentsMap;

        MAKE_GETTER_SETTER_MT(ComponentsMap,
                              ComponentsMap,
                              m_components,
                              m_compMapMutex)

        MAKE_GETTER_SETTER_MT(SimDriverState,
                              State,
                              m_state,
                              m_stateMutex)

        template <typename TComp>
        std::shared_ptr<TComp> getComponent(const UUID &id) const {
            std::lock_guard lk(m_compMapMutex);

            if (!m_components.contains(id)) {
                return nullptr;
            }
            return std::dynamic_pointer_cast<TComp>(
                m_components.at(id));
        }

        bool isInitialized() const {
            std::lock_guard lk(m_stateMutex);
            return m_state != SimDriverState::uninitialized;
        }

        bool isRunning() const {
            std::lock_guard lk(m_stateMutex);
            return m_state == SimDriverState::running;
        }

        bool isPaused() const {
            std::lock_guard lk(m_stateMutex);
            return m_state == SimDriverState::paused;
        }

        bool isStopped() const {
            std::lock_guard lk(m_stateMutex);
            return m_state == SimDriverState::stopped;
        }

        bool isDestroyed() const {
            std::lock_guard lk(m_stateMutex);
            return m_state == SimDriverState::destroyed;
        }

        void pause() {
            if (isRunning()) {
                onPause();
                std::lock_guard lk(m_stateMutex);
                m_state = SimDriverState::paused;
            }
        }

        void resume() {
            if (isPaused()) {
                onResume();
                std::lock_guard lk(m_stateMutex);
                m_state = SimDriverState::running;
            }
        }

        void stop() {
            if (isRunning() || isPaused()) {
                onStop();
                std::lock_guard lk(m_stateMutex);
                m_state = SimDriverState::stopped;
            }
        }

        void reset() {
            onReset();
            std::lock_guard lk(m_stateMutex);
            m_state = SimDriverState::uninitialized;
        }

        void destroy() {
            if (isDestroyed())
                return;

            onDestroy();
            std::lock_guard lk(m_stateMutex);
            m_state = SimDriverState::destroyed;
        }

        void step() {
            if (isPaused()) {
                onStep();
            }
        }

      protected:
        ComponentsMap m_components;
        SimDriverState m_state = SimDriverState::uninitialized;
        mutable std::mutex m_compMapMutex;
        mutable std::mutex m_stateMutex;
    };
} // namespace Bess::SimEngine::Drivers
