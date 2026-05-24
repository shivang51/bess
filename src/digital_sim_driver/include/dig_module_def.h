#pragma once
#include "common/bess_api.h"
#include "common/bess_uuid.h"
#include "dig_sim_driver.h"
#include <memory>

namespace Bess::SimEngine {
    class BESS_API ModuleDefinition : public Drivers::Digital::DigCompDef {
      public:
        static constexpr const char *TypeName = "dig_mod_compdef";

        static std::shared_ptr<ModuleDefinition> createNew();

        std::string getTypeName() const override;

        std::shared_ptr<Drivers::CompDef> clone() const override;

        typedef std::shared_ptr<Drivers::Digital::DigCompSimData>
            TDigSimFnDataPtr;
        TDigSimFnDataPtr simFunction(const TDigSimFnDataPtr &data);

        MAKE_GETTER_SETTER(UUID, InputId, m_input)
        MAKE_GETTER_SETTER(UUID, OutputId, m_output)

        Json::Value toJson() const override;

        void loadJson(const Json::Value &json) override;

      private:
        UUID m_input = UUID::null, m_output = UUID::null;
    };
} // namespace Bess::SimEngine
