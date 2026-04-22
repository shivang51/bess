#pragma once

#include "event_based_sim_driver.h"
#include "types.h"

namespace Bess::SimEngine::Digital {

    struct BESS_API DigCompState {
        std::vector<SlotState> inputStates;
        std::vector<SlotState> outputStates;
    };

    struct BESS_API DigCompSimData {
        std::vector<SlotState> inputStates;
        DigCompState prevState;
        TimeNs simTime;
    };

    class BESS_API DigitalSimComponent : public EvtBasedSimComponent<DigCompSimData> {
      public:
        DigitalSimComponent() = default;
        ~DigitalSimComponent() override = default;

        MAKE_GETTER_SETTER(std::vector<SlotState>, InputStates, m_inputStates)
        MAKE_GETTER_SETTER(std::vector<SlotState>, OutputStates, m_outputStates)

        Json::Value toJson() const override {
            Json::Value json = EvtBasedSimComponent<DigCompSimData>::toJson();
            return json;
        }

        static void fromJson(const std::shared_ptr<DigitalSimComponent> &comp,
                             const Json::Value &json);

      private:
        std::vector<SlotState> m_inputStates;
        std::vector<SlotState> m_outputStates;
    };

    class BESS_API DigitalSimDriver : public EvtBasedSimDriver<DigCompSimData> {
      public:
        DigitalSimDriver() = default;
        ~DigitalSimDriver() override = default;

        bool simulate(const UUID &id) override {
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

      private:
        bool simulateComponent(const SimEvt &evt);
    };
} // namespace Bess::SimEngine::Digital
