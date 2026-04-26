#pragma once

#include "common/bess_uuid.h"
#include "component_definition.h"
#include "drivers/sim_driver.h"
#include "event_based_sim_driver.h"
#include "types.h"
#include <memory>

namespace Bess::SimEngine::Drivers::Digital {

    struct BESS_API DigCompState {
        std::vector<SlotState> inputStates;
        std::vector<SlotState> outputStates;
    };

    struct BESS_API DigCompSimData : SimFnDataBase {
        std::vector<SlotState> inputStates;
        std::vector<SlotState> outputStates;
        std::vector<std::string> *expressions = nullptr;
        TimeNs simTime;
        DigCompState prevState;
    };

    class BESS_API DigCompDef : public EvtBasedCompDef {
      public:
        static constexpr const char *TypeName = "digital_compdef";

        DigCompDef() = default;

        typedef std::function<std::shared_ptr<DigCompSimData>(
            const std::shared_ptr<DigCompSimData> &)>
            TDigSimFn;

        DigCompDef(const DigCompDef &other) = default;
        DigCompDef(DigCompDef &&) = default;

        ~DigCompDef() override = default;

        MAKE_GETTER_SETTER(SlotsGroupInfo, InputSlotsInfo, m_inputSlotsInfo)
        MAKE_GETTER_SETTER(SlotsGroupInfo, OutputSlotsInfo, m_outputSlotsInfo)
        MAKE_GETTER_SETTER(OperatorInfo, OpInfo, m_opInfo)
        MAKE_GETTER_SETTER(ComponentBehaviorType, BehaviorType, m_behaviorType)
        MAKE_GETTER_SETTER_WC(std::vector<std::string>,
                              OutputExpressions,
                              m_outputExpressions,
                              onExpressionsChange)

        void setSimFn(const TDigSimFn &simFn);

        Json::Value toJson() const override;

        virtual bool computeExpressionsIfNeeded();

        std::string getTypeName() const override;

        std::shared_ptr<CompDef> clone() const override;

      public:
        /**
         * This function will be called when resize of the slots group is requested;
         * groupType: SlotsGroupType, type of slots group, e.g. input or output;
         * newSize: size_t, new size of the group;
         * Should return true if newSize is acceptable otherwise false;
         * returning false will reject the resize request and it will not change;
         * Note: if not overrriden and implemented,
         * by default it will return value of group.isResizeable
         **/
        virtual bool onSlotsResizeReq(SlotsGroupType groupType, size_t newSize);

        /**
         * This function will be called when the component state changes;
         * oldState: previous state of the component;
         * newState: new state of the component;
         **/
        virtual void onStateChange(const ComponentState &oldState,
                                   const ComponentState &newState);

        virtual void onExpressionsChange();

        friend bool operator==(ComponentDefinition &a, ComponentDefinition &b) noexcept {
            return a.getHash() == b.getHash();
        }

        friend bool operator==(const std::shared_ptr<ComponentDefinition> &a,
                               const std::shared_ptr<ComponentDefinition> &b) noexcept {
            return a->getHash() == b->getHash();
        }

      protected:
        SlotsGroupInfo m_inputSlotsInfo{}, m_outputSlotsInfo{};
        OperatorInfo m_opInfo{};
        ComponentBehaviorType m_behaviorType = ComponentBehaviorType::none;
        std::vector<std::string> m_outputExpressions; // A+B or A.B etc.
    };

    class BESS_API DigSimComp : public EvtBasedSimComp {
      public:
        DigSimComp() = default;
        ~DigSimComp() override = default;

        static std::shared_ptr<DigSimComp> fromDef(
            const std::shared_ptr<CompDef> &compDef) {
            if (!compDef) {
                BESS_WARN("(DigitalSimDriver.fromDef) compDef is nullptr");
                return nullptr;
            }

            const auto clone = compDef->clone();

            const auto comp = std::make_shared<DigSimComp>();
            comp->setName(clone->getName());
            comp->setDefinition(clone);

            auto digDef = std::dynamic_pointer_cast<DigCompDef>(clone);
            const auto inpCount = digDef->getInputSlotsInfo().count;
            const auto outCount = digDef->getOutputSlotsInfo().count;

            comp->m_inputStates.resize(inpCount);
            comp->m_outputStates.resize(outCount);

            comp->m_isInputConnected.resize(inpCount, false);
            comp->m_isOutputConnected.resize(outCount, false);

            comp->m_inputConnections.resize(inpCount);
            comp->m_outputConnections.resize(outCount);

            return comp;
        }

        MAKE_GETTER_SETTER(std::vector<SlotState>, InputStates, m_inputStates)
        MAKE_GETTER_SETTER(std::vector<SlotState>, OutputStates, m_outputStates)
        MAKE_GETTER_SETTER(Connections, InputConnections, m_inputConnections)
        MAKE_GETTER_SETTER(Connections, OutputConnections, m_outputConnections)
        MAKE_GETTER_SETTER(std::vector<bool>, IsInputConnected, m_isInputConnected)
        MAKE_GETTER_SETTER(std::vector<bool>, IsOutputConnected, m_isOutputConnected)
        MAKE_GETTER_SETTER(UUID, NetUuid, m_netUuid)

        Json::Value toJson() const override {
            Json::Value json = EvtBasedSimComp::toJson();
            JsonConvert::toJsonValue(m_inputStates, json["inputStates"]);
            JsonConvert::toJsonValue(m_outputStates, json["outputStates"]);
            JsonConvert::toJsonValue(m_inputConnections, json["inputConnections"]);
            JsonConvert::toJsonValue(m_outputConnections, json["outputConnections"]);
            JsonConvert::toJsonValue(m_isInputConnected, json["isInputConnected"]);
            JsonConvert::toJsonValue(m_isOutputConnected, json["isOutputConnected"]);
            JsonConvert::toJsonValue(m_netUuid, json["netUuid"]);
            return json;
        }

        static void fromJson(const std::shared_ptr<DigSimComp> &comp,
                             const Json::Value &json);

      private:
        std::vector<SlotState> m_inputStates;
        std::vector<SlotState> m_outputStates;
        Connections m_inputConnections;
        Connections m_outputConnections;
        std::vector<bool> m_isInputConnected;
        std::vector<bool> m_isOutputConnected;
        UUID m_netUuid = UUID::null;
    };

    class BESS_API DigitalSimDriver final : public EvtBasedSimDriver {
      public:
        DigitalSimDriver() = default;
        ~DigitalSimDriver() override = default;

        std::shared_ptr<SimComponent> createComp(const std::shared_ptr<CompDef> &def) override;

        void onComponentAdded(const std::shared_ptr<SimComponent> &comp) override;

        void deleteComponent(const UUID &uuid) override;

        void clearComponents() override;

        bool suuportsDef(const std::shared_ptr<CompDef> &def) const override {
            return std::dynamic_pointer_cast<DigCompDef>(def) != nullptr;
        }

        std::string getName() const override;

        bool simulate(const SimEvt &evt) override;

        void addComponent(const std::shared_ptr<DigSimComp> &comp,
                          bool scheduleSim = true);

        void onBeforeRun() override;

        std::pair<bool, std::string> canConnectComponents(
            const UUID &src, int srcSlotIdx, SlotType srcType,
            const UUID &dst, int dstSlotIdx, SlotType dstType) const override;

        bool connectComponent(
            const UUID &src, int srcSlotIdx, SlotType srcType,
            const UUID &dst, int dstSlotIdx, SlotType dstType, bool overrideConn) override;

        void deleteConnection(
            const UUID &compA, SlotType pinAType, int idxA,
            const UUID &compB, SlotType pinBType, int idxB) override;

        bool addSlot(const UUID &compId, SlotType type, int index) override;
        bool removeSlot(const UUID &compId, SlotType type, int index) override;

        ConnectionBundle getConnections(const UUID &uuid) const override;
        std::vector<UUID> getDependants(const UUID &id) override;
        std::vector<SlotState> collapseInputs(const UUID &id) override;
        std::vector<SlotState> getInputSlotsState(const UUID &compId) override;
        SlotState getSlotState(const UUID &uuid, SlotType type, int idx) const override;
        bool setInputSlotState(const UUID &uuid, int pinIdx, LogicState state) override;
        bool setOutputSlotState(const UUID &uuid, int pinIdx, LogicState state) override;
        ComponentState getComponentState(const UUID &uuid) const override;

        const std::unordered_map<UUID, Net> &getNetsMap() const override;
        bool isNetUpdated() const override;
        void clearNetUpdated() override;

      private:
        std::unordered_map<UUID, Net> m_nets;
        bool m_isNetUpdated{false};
    };

} // namespace Bess::SimEngine::Drivers::Digital

namespace Bess::JsonConvert {

    void toJsonValue(Json::Value &json,
                     const Bess::SimEngine::Drivers::Digital::DigSimComp &data);
} // namespace Bess::JsonConvert
