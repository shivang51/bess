#pragma once
#include "common/class_helpers.h"
#include "common/types.h"
#include "net/net.h"
#include <common/bess_uuid.h>
#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace Bess::SimEngine::Drivers {

    class SimFnDataBase {
      public:
        virtual ~SimFnDataBase() = default;
        bool simDependants = false;
    };

    class CompDef {
      public:
        typedef std::shared_ptr<SimFnDataBase> SimFnDataPtr;
        typedef std::function<SimFnDataPtr(const SimFnDataPtr &)> SimFn;

        CompDef() = default;
        virtual ~CompDef() = default;

        MAKE_GETTER_SETTER(std::string, Name, m_name)
        MAKE_GETTER_SETTER(std::string, GroupName, m_groupName)
        MAKE_VGETTER_VSETTER(SimFn, SimFn, m_simFn)

        virtual Json::Value toJson() const;

        virtual void loadJson(const Json::Value &json);

        virtual std::shared_ptr<CompDef> clone() const = 0;

        virtual std::string getTypeName() const = 0;

      protected:
        std::string m_name;
        std::string m_groupName;
        SimFn m_simFn = nullptr;
    };

    class SimComponent {
      public:
        SimComponent() = default;
        virtual ~SimComponent() = default;

        template <typename TCompDef>
        std::shared_ptr<TCompDef> getDefinition() const {
            return std::dynamic_pointer_cast<TCompDef>(m_def);
        }

        MAKE_GETTER_SETTER(UUID, Uuid, m_uuid)
        MAKE_GETTER_SETTER(std::string, Name, m_name)
        MAKE_GETTER_SETTER(std::shared_ptr<CompDef>, Definition, m_def)

        virtual Json::Value toJson() const;
        virtual void loadJson(const Json::Value &json);

        virtual void onPostSimulate() {
        }

        virtual std::shared_ptr<SimFnDataBase>
        simulate(const std::shared_ptr<SimFnDataBase> &data);

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

    // comp id, slot type, new count
    typedef std::function<void(const UUID &, SlotType, int)> SlotCountChangeCB;

    struct SlotsCountChangeRes {
        bool changedInp = false;
        bool changedOut = false;

        static SlotsCountChangeRes noChange() {
            return SlotsCountChangeRes{false, false};
        }

        static SlotsCountChangeRes inpChanged() {
            return SlotsCountChangeRes{true, false};
        }

        static SlotsCountChangeRes outChanged() {
            return SlotsCountChangeRes{false, true};
        }

        static SlotsCountChangeRes bothChanged() {
            return SlotsCountChangeRes{true, true};
        }

        bool hasChange() const {
            return changedInp || changedOut;
        }
    };

    class SimDriver {
      public:
        SimDriver() = default;
        virtual ~SimDriver() = default;

        // will be ran in seperate thread
        virtual void run() = 0;

        virtual std::string getName() const = 0;

        // returns whether driver will accept the component.
        virtual bool supportsDef(const std::shared_ptr<CompDef> &def) const = 0;

        virtual std::shared_ptr<SimComponent>
        createComp(const std::shared_ptr<CompDef> &def,
                   bool cloneDef = true) = 0;

        virtual UUID addComponent(const std::shared_ptr<SimComponent> &comp,
                                  bool scheduleSim);

        virtual void deleteComponent(const UUID &uuid);

        virtual void clearComponents();

        virtual bool isSimStable() const;

        virtual void clearPendingEvents() {
        }

        // Connection management
        virtual std::pair<bool, std::string>
        canConnectComponents(const UUID &src,
                             int srcSlotIdx,
                             SlotType srcType,
                             const UUID &dst,
                             int dstSlotIdx,
                             SlotType dstType) const = 0;

        virtual bool connectComponent(const UUID &src,
                                      int srcSlotIdx,
                                      SlotType srcType,
                                      const UUID &dst,
                                      int dstSlotIdx,
                                      SlotType dstType,
                                      bool overrideConn) = 0;

        virtual void deleteConnection(const UUID &compA,
                                      SlotType pinAType,
                                      int idxA,
                                      const UUID &compB,
                                      SlotType pinBType,
                                      int idxB) = 0;

        virtual SlotsCountChangeRes addSlot(const UUID &compId,
                                            SlotType type,
                                            int index,
                                            bool force = false) = 0;

        virtual SlotsCountChangeRes removeSlot(const UUID &compId,
                                               SlotType type,
                                               int index,
                                               bool force = false) = 0;

        virtual ConnectionBundle getConnections(const UUID &uuid) const;

        virtual std::vector<SlotState> getInputSlotsState(const UUID &compId);

        virtual SlotState
        getSlotState(const UUID &uuid, SlotType type, int idx) const;

        virtual bool
        setInputSlotState(const UUID &uuid, int pinIdx, LogicState state);

        virtual bool
        setOutputSlotState(const UUID &uuid, int pinIdx, LogicState state);

        virtual ComponentState getComponentState(const UUID &uuid) const;

        virtual void propagateFromComponent(const UUID &sourceId);

        virtual const std::unordered_map<UUID, Net> &getNetsMap() const;

        virtual bool isNetUpdated() const;

        virtual void clearNetUpdated();

        virtual Json::Value toJson() const = 0;

        virtual void loadJson(const Json::Value &json) = 0;

        void addOnSlotCountChangeCB(const UUID &id,
                                    const SlotCountChangeCB &cb);

        void removeOnSlotCountChangeCB(const UUID &id);

      protected:
        virtual void
        onComponentAdded(const std::shared_ptr<SimComponent> &comp) {
        }

        virtual void onInit() {};

        virtual void onStop() {};

        virtual void onReset() {};

        virtual void onPause() {};

        virtual void onResume() {};

        virtual void onStep() {};

        virtual void onDestroy() {};

        void triggerSlotCountChangeCbs(const UUID &compId,
                                       SlotType type,
                                       int newCount);

      public:
        typedef std::shared_ptr<SimComponent> SimComponentPtr;
        typedef HashMap<UUID, SimComponentPtr> ComponentsMap;

        MAKE_GETTER_SETTER_MT(ComponentsMap,
                              ComponentsMap,
                              m_components,
                              m_compMapMutex)

        MAKE_GETTER_SETTER_MT(SimDriverState, State, m_state, m_stateMutex)

        bool hasComponent(const UUID &id) const;

        template <typename TComp>
        std::shared_ptr<TComp> getComponentSP(const UUID &id) const {
            std::lock_guard lk(m_compMapMutex);

            auto it = m_components.find(id);
            if (it == m_components.end()) {
                return nullptr;
            }
            return std::dynamic_pointer_cast<TComp>(it->second);
        }

        // For hotpaths
        template <typename TComp> TComp *getComponent(const UUID &id) const {
            std::lock_guard lk(m_compMapMutex);

            auto it = m_components.find(id);
            if (it == m_components.end()) {
                return nullptr;
            }
            return static_cast<TComp *>(it->second.get());
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

        std::unordered_map<UUID, SlotCountChangeCB> m_onSlotCountChnageCBs;
    };
} // namespace Bess::SimEngine::Drivers
