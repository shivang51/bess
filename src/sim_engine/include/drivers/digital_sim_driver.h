#pragma once

#include "component_definition.h"
#include "event_based_sim_driver.h"
#include "types.h"

namespace Bess::SimEngine::Drivers::Digital {

    struct BESS_API DigCompState {
        std::vector<SlotState> inputStates;
        std::vector<SlotState> outputStates;
    };

    struct BESS_API DigCompSimData : SimFnDataBase {
        std::vector<SlotState> inputStates;
        DigCompState prevState;
        TimeNs simTime;
    };

    class BESS_API DigCompDef : public EvtBasedCompDef {
      public:
        DigCompDef() = default;

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

        /**
         * This function will compute the output expressions if needed.
         * i.e. when operator info is set and expressions are based on it.
         * returns: true if expressions were computed, false otherwise;
         **/
        virtual bool computeExpressionsIfNeeded() {
            return false;
        }

        // callbacks
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
        virtual bool onSlotsResizeReq(SlotsGroupType groupType, size_t newSize) {
            return false;
        }

        /**
         * This function will be called when the component state changes;
         * oldState: previous state of the component;
         * newState: new state of the component;
         **/
        virtual void onStateChange(const ComponentState &oldState,
                                   const ComponentState &newState) {}

        virtual void onExpressionsChange() {}

        std::shared_ptr<ComponentDef> clone() const override {
            return std::make_shared<DigCompDef>(*this);
        }

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

    class BESS_API DigitalSimComponent : public EvtBasedSimComponent {
      public:
        DigitalSimComponent() = default;
        ~DigitalSimComponent() override = default;

        MAKE_GETTER_SETTER(std::vector<SlotState>, InputStates, m_inputStates)
        MAKE_GETTER_SETTER(std::vector<SlotState>, OutputStates, m_outputStates)

        Json::Value toJson() const override {
            Json::Value json = EvtBasedSimComponent::toJson();
            JsonConvert::toJsonValue(m_inputStates, json["inputStates"]);
            JsonConvert::toJsonValue(m_outputStates, json["outputStates"]);
            return json;
        }

        static void fromJson(const std::shared_ptr<DigitalSimComponent> &comp,
                             const Json::Value &json);

      private:
        std::vector<SlotState> m_inputStates;
        std::vector<SlotState> m_outputStates;
    };

    class BESS_API DigitalSimDriver : public EvtBasedSimDriver {
      public:
        DigitalSimDriver() = default;
        ~DigitalSimDriver() override = default;

        std::shared_ptr<DigitalSimComponent> fromDef(
            const std::shared_ptr<ComponentDef> &compDef) const {
            if (!compDef) {
                BESS_WARN("(DigitalSimDriver.fromDef) compDef is nullptr");
                return nullptr;
            }

            const auto clone = compDef->clone();

            const auto comp = std::make_shared<DigitalSimComponent>();
            comp->setName(clone->getName());
            comp->setDefinition(clone);

            return comp;
        }

        bool simulate(const SimEvt &evt) override {
            const auto &id = evt.compId;

            const auto &comp = this->template getComponent<DigitalSimComponent>(id);

            if (!comp) {
                BESS_WARN("(DigitalSimDriver.simulate) Component with UUID {} not found",
                          (uint64_t)id);
                return false;
            }

            DigCompSimData simData{};
            simData.simTime = m_currentSimTime;
            simData.prevState.inputStates = comp->getInputStates();
            simData.prevState.outputStates = comp->getOutputStates();
            simData.inputStates = collapseInputs(id);

            return comp->simulate(simData);
        }

        void addComponent(const std::shared_ptr<DigitalSimComponent> &comp) {
            EvtBasedSimDriver::addComponent(comp);
        }
    };
} // namespace Bess::SimEngine::Drivers::Digital
