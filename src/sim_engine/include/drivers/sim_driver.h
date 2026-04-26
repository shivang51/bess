#pragma once
#include "bess_api.h"
#include "common/class_helpers.h"
#include "net/net.h"
#include "types.h"
#include <common/bess_uuid.h>
#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace Bess::SimEngine::Drivers {

    class BESS_API SimFnDataBase {
      public:
        virtual ~SimFnDataBase() = default;
        bool simDependants = false;
    };

    class BESS_API CompDef {
      public:
        typedef std::shared_ptr<SimFnDataBase> SimFnDataPtr;
        typedef std::function<SimFnDataPtr(const SimFnDataPtr &)> SimFn;

        CompDef() = default;
        virtual ~CompDef() = default;

        MAKE_GETTER_SETTER(std::string, Name, m_name)
        MAKE_GETTER_SETTER(std::string, GroupName, m_groupName)
        MAKE_VGETTER_VSETTER(SimFn, SimFn, m_simFn)

        virtual Json::Value toJson() const;

        static void fromJson(const std::shared_ptr<CompDef> &compDef,
                             const Json::Value &json) {}

        virtual std::shared_ptr<CompDef> clone() const = 0;

        virtual std::string getTypeName() const = 0;

      protected:
        std::string m_name;
        std::string m_groupName;
        SimFn m_simFn = nullptr;
    };

    class BESS_API SimComponent {
      public:
        SimComponent() = default;
        virtual ~SimComponent() = default;

        template <typename TCompDef>
        std::shared_ptr<TCompDef> getDefinition() const {
            return std::dynamic_pointer_cast<TCompDef>(m_def);
        }

        MAKE_GETTER_SETTER(UUID, Uuid, m_uuid)
        MAKE_GETTER_SETTER(std::string, Name, m_name)
        MAKE_GETTER_SETTER(std::shared_ptr<CompDef>,
                           Definition,
                           m_def)

        virtual Json::Value toJson() const;

        static void fromJson(const std::shared_ptr<SimComponent> &comp,
                             const Json::Value &json);

        virtual std::shared_ptr<SimFnDataBase> simulate(
            const std::shared_ptr<SimFnDataBase> &data);

      protected:
        UUID m_uuid; // will auto gen id for each instance
        std::string m_name;
        std::shared_ptr<CompDef> m_def = nullptr;
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

        // will be ran in seperate thread
        virtual void run() = 0;

        virtual std::string getName() const = 0;

        // returns whether driver will accept the component.
        virtual bool suuportsDef(const std::shared_ptr<CompDef> &def) const = 0;

        virtual std::shared_ptr<SimComponent> createComp(
            const std::shared_ptr<CompDef> &def) = 0;

        virtual void onComponentAdded(const std::shared_ptr<SimComponent> &comp) {}

        virtual void deleteComponent(const UUID &uuid);

        virtual void clearComponents();

        virtual void clearPendingEvents() {}

        // Connection management
        virtual std::pair<bool, std::string> canConnectComponents(
            const UUID &src, int srcSlotIdx, SlotType srcType,
            const UUID &dst, int dstSlotIdx, SlotType dstType) const = 0;

        virtual bool connectComponent(
            const UUID &src, int srcSlotIdx, SlotType srcType,
            const UUID &dst, int dstSlotIdx, SlotType dstType, bool overrideConn) = 0;

        virtual void deleteConnection(
            const UUID &compA, SlotType pinAType, int idxA,
            const UUID &compB, SlotType pinBType, int idxB) = 0;

        virtual bool addSlot(const UUID &compId, SlotType type, int index) = 0;

        virtual bool removeSlot(const UUID &compId, SlotType type, int index) = 0;

        virtual ConnectionBundle getConnections(const UUID &uuid) const;

        virtual std::vector<SlotState> getInputSlotsState(const UUID &compId);

        virtual SlotState getSlotState(const UUID &uuid, SlotType type, int idx) const;

        virtual bool setInputSlotState(const UUID &uuid, int pinIdx, LogicState state);

        virtual bool setOutputSlotState(const UUID &uuid, int pinIdx, LogicState state);

        virtual ComponentState getComponentState(const UUID &uuid) const;

        virtual void propagateFromComponent(const UUID &sourceId);

        virtual const std::unordered_map<UUID, Net> &getNetsMap() const;

        virtual bool isNetUpdated() const;

        virtual void clearNetUpdated();

      protected:
        virtual void onInit() {};

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

        bool hasComponent(const UUID &id) const;

        template <typename TComp>
        std::shared_ptr<TComp> getComponent(const UUID &id) const {
            std::lock_guard lk(m_compMapMutex);

            if (!m_components.contains(id)) {
                return nullptr;
            }
            return std::dynamic_pointer_cast<TComp>(
                m_components.at(id));
        }

        void init();

        bool isInitialized() const;

        bool isRunning() const;

        bool isPaused() const;

        bool isStopped() const;

        bool isDestroyed() const;

        void pause();

        void resume();

        void stop();

        void reset();

        void destroy();

        void step();

      protected:
        ComponentsMap m_components;
        SimDriverState m_state = SimDriverState::uninitialized;
        mutable std::mutex m_compMapMutex;
        mutable std::mutex m_stateMutex;
    };
} // namespace Bess::SimEngine::Drivers
