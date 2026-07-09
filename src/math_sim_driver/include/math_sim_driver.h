#pragma once

#include "common/bess_uuid.h"
#include "common/types.h"
#include "net/net.h"
#include "sim_driver/event_based_sim_driver.h"
#include "sim_driver/sim_driver.h"
#include "json/value.h"

#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace Bess::SimEngine::Drivers::Math {

    enum class MathOpKind : uint8_t { none, add, subtract };

    struct MathCompState {
        std::vector<PortState> inputStates;
        std::vector<PortState> outputStates;
    };

    struct MathCompSimData : SimFnDataBase {
        std::vector<PortState> inputStates;
        std::vector<PortState> outputStates;
        SimTime simTime;
        MathCompState prevState;
    };

    class MathCompDef : public EvtBasedCompDef {
      public:
        static constexpr const char *TypeName = "math_compdef";

        using TScalarFn = std::function<double(const std::vector<double> &)>;
        using TMathSimFn = std::function<std::shared_ptr<MathCompSimData>(
            const std::shared_ptr<MathCompSimData> &)>;

        MathCompDef();
        ~MathCompDef() override = default;

        static std::shared_ptr<MathCompDef>
        makeBinaryOp(const std::string &name,
                     const std::string &groupName,
                     MathOpKind opKind,
                     TimeNs propDelay = TimeNs(0));

        MAKE_GETTER_SETTER(MathOpKind, OpKind, m_opKind)
        MAKE_GETTER(TScalarFn, ScalarFn, m_scalarFn)

        void setInputPortDescriptor(const PortDescriptor &descriptor);
        void setOutputPortDescriptor(const PortDescriptor &descriptor);
        void setSimFn(const TMathSimFn &simFn);
        void setScalarFn(const TScalarFn &scalarFn);

        PortDescriptor getInputPortDescriptor() const override;
        PortDescriptor getOutputPortDescriptor() const override;

        Json::Value toJson() const override;
        void loadJson(const Json::Value &json) override;

        std::string getTypeName() const override;
        std::shared_ptr<CompDef> clone() const override;

        bool computeScalarFnIfNeeded();

      private:
        PortDescriptor m_inputPorts;
        PortDescriptor m_outputPorts;
        MathOpKind m_opKind = MathOpKind::none;
        TScalarFn m_scalarFn = nullptr;
    };

    class MathSimComp : public EvtBasedSimComp {
      public:
        MathSimComp() = default;
        ~MathSimComp() override = default;

        template <typename TComp>
        static std::shared_ptr<TComp>
        fromDef(const std::shared_ptr<CompDef> &compDef, bool cloneDef = true)
            requires(std::is_base_of_v<MathSimComp, TComp>)
        {
            if (!compDef) {
                return nullptr;
            }

            const auto clone = cloneDef ? compDef->clone() : compDef;
            const auto mathDef = std::dynamic_pointer_cast<MathCompDef>(clone);
            if (!mathDef) {
                return nullptr;
            }

            const auto comp = std::make_shared<TComp>();
            comp->setName(clone->getName());
            comp->setDefinition(clone);

            const auto inputCount = mathDef->getInputPortDescriptor().count;
            const auto outputCount = mathDef->getOutputPortDescriptor().count;

            comp->m_inputStates.resize(inputCount);
            comp->m_outputStates.resize(outputCount);
            comp->m_isInputConnected.resize(inputCount, false);
            comp->m_isOutputConnected.resize(outputCount, false);
            comp->m_inputConnections.resize(inputCount);
            comp->m_outputConnections.resize(outputCount);

            return comp;
        }

        static std::shared_ptr<MathSimComp>
        fromDef(const std::shared_ptr<CompDef> &compDef, bool cloneDef = true);

        MAKE_GETTER_SETTER(std::vector<PortState>, InputStates, m_inputStates)
        MAKE_GETTER_SETTER(std::vector<PortState>, OutputStates, m_outputStates)
        MAKE_GETTER_SETTER(Connections, InputConnections, m_inputConnections)
        MAKE_GETTER_SETTER(Connections, OutputConnections, m_outputConnections)
        MAKE_GETTER_SETTER(std::vector<bool>,
                           IsInputConnected,
                           m_isInputConnected)
        MAKE_GETTER_SETTER(std::vector<bool>,
                           IsOutputConnected,
                           m_isOutputConnected)
        MAKE_GETTER_SETTER(UUID, NetUuid, m_netUuid)

        Json::Value toJson() const override;
        void loadJson(const Json::Value &json) override;

      private:
        std::vector<PortState> m_inputStates;
        std::vector<PortState> m_outputStates;
        Connections m_inputConnections;
        Connections m_outputConnections;
        std::vector<bool> m_isInputConnected;
        std::vector<bool> m_isOutputConnected;
        UUID m_netUuid = UUID::null;
    };

    class MathSimDriver final : public EvtBasedSimDriver {
      public:
        MathSimDriver() = default;
        ~MathSimDriver() override = default;

        static constexpr const char *NAME = "mathsimdriver";

        std::shared_ptr<SimComponent>
        createComp(const std::shared_ptr<CompDef> &def,
                   bool cloneDef = true) override;

        void
        onComponentAdded(const std::shared_ptr<SimComponent> &comp) override;

        void deleteComponent(const UUID &uuid) override;
        void clearComponents() override;

        bool supportsDef(const std::shared_ptr<CompDef> &def) const override {
            return std::dynamic_pointer_cast<MathCompDef>(def) != nullptr;
        }

        std::string getName() const override;

        bool simulate(const SimEvt &evt,
                      const std::vector<PortState> &inputs) override;

        UUID addComponent(const std::shared_ptr<SimComponent> &comp,
                          bool scheduleSim = true) override;

        bool isSimStable() const override;

        std::pair<bool, std::string>
        canConnectPorts(const PortRef &src,
                        const PortRef &dst) const override;

        bool connectPorts(const PortRef &src,
                          const PortRef &dst,
                          bool overrideConn) override;

        void deleteConnection(const PortRef &portA,
                              const PortRef &portB) override;

        PortCountChangeRes addPort(const PortRef &port,
                                   bool force = false) override;
        PortCountChangeRes removePort(const PortRef &port,
                                      bool force = false) override;

        ConnectionBundle getConnections(const UUID &uuid) const override;
        std::vector<UUID> getDependants(const UUID &id) override;
        std::vector<PortState> collapseInputs(const UUID &id) override;
        std::vector<PortState> getInputPortStates(const UUID &compId) override;
        PortState getPortState(const PortRef &port) const override;
        bool setInputPortState(const UUID &uuid,
                               int pinIdx,
                               const PortState &state) override;
        bool setOutputPortState(const UUID &uuid,
                                int pinIdx,
                                const PortState &state) override;
        ComponentState getComponentState(const UUID &uuid) const override;

        const std::unordered_map<UUID, Net> &getNetsMap() const override;
        bool isNetUpdated() const override;
        void clearNetUpdated() override;

        Json::Value toJson() const override;
        void loadJson(const Json::Value &json) override;

        void onInit() override;

      private:
        std::unordered_map<UUID, Net> m_nets;
        bool m_isNetUpdated{false};
    };

    void registerMathSimDriver();
    void unregisterMathSimDriver();

} // namespace Bess::SimEngine::Drivers::Math

namespace Bess::JsonConvert {
    void toJsonValue(Json::Value &json,
                     const Bess::SimEngine::Drivers::Math::MathSimComp &data);
} // namespace Bess::JsonConvert
