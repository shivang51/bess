#pragma once
#include "bess_api.h"
#include "common/class_helpers.h"
#include <common/bess_uuid.h>
#include <mutex>
#include <unordered_map>

namespace Bess::SimEngine {

    template <typename TData>
    class BESS_API SimComponent {
      public:
        typedef std::function<bool(const TData &)> SimFn;

        SimComponent() = default;
        virtual ~SimComponent() = default;

        MAKE_GETTER_SETTER(UUID, uuid, m_uuid)
        MAKE_GETTER_SETTER(std::string, name, m_name)
        MAKE_GETTER_SETTER(SimFn, onSimulate, m_onSimulate)

        virtual Json::Value toJson() const {
            Json::Value json;
            JsonConvert::toJsonValue(m_uuid, json["uuid"]);
            json["name"] = m_name;
            return json;
        }
        static void fromJson(const std::shared_ptr<SimComponent> &comp,
                             const Json::Value &json);

        virtual bool simulate(const TData &data) {
            if (m_onSimulate) {
                return m_onSimulate(data);
            }
            return false;
        }

      private:
        UUID m_uuid = UUID::null;
        std::string m_name;
        SimFn m_onSimulate = nullptr;
    };

    enum class SimDriverState : uint8_t {
        uninitialized,
        destroyed,
        stopped,
        running,
        paused
    };

    template <typename TData>
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
        typedef std::shared_ptr<SimComponent<TData>> SimComponentPtr;
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

      private:
        ComponentsMap m_components;
        SimDriverState m_state = SimDriverState::uninitialized;

        std::mutex m_compMapMutex;
        std::mutex m_stateMutex;
    };
} // namespace Bess::SimEngine
