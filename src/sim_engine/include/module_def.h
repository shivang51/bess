#pragma once
#include "bess_api.h"
#include "common/bess_uuid.h"
#include "component_definition.h"
#include "drivers/digital_sim_driver.h"
#include <memory>

namespace Bess::SimEngine {
    class BESS_API ModuleDefinition : public Drivers::Digital::DigCompDef {
      public:
        static std::shared_ptr<ModuleDefinition> createNew();

        // FIXME: MoudelDef clone
        // std::shared_ptr<ComponentDefinition> clone() const override;

        ComponentState simulationFunction(const std::vector<SlotState> &inputs,
                                          SimTime simTime,
                                          const ComponentState &prevState);

        MAKE_GETTER_SETTER(UUID, InputId, m_input)
        MAKE_GETTER_SETTER(UUID, OutputId, m_output)

      private:
        UUID m_input = UUID::null, m_output = UUID::null;
    };
} // namespace Bess::SimEngine

// REFLECT_DERIVED_PROPS(Bess::SimEngine::ModuleDefinition,
//                       Bess::SimEngine::ComponentDefinition,
//                       ("input", getInputId, setInputId),
//                       ("output", getOutputId, setOutputId))

// REFLECT_PROPS_SP(Bess::SimEngine::ModuleDefinition);
