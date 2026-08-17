#include "math_sim_driver.h"

#include "common/bess_assert.h"
#include "common/logger.h"
#include "component_catalog.h"
#include "driver_registry.h"
#include "net/net_rebuilder.h"
#include "json/value.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <ranges>
#include <stdexcept>
#include <string>

namespace Bess::SimEngine::Drivers::Math {

    void registerMathSimDriver() {
        DriverRegistry::registerDriver(MathSimDriver::NAME, []() {
            return std::make_shared<MathSimDriver>();
        });
    }

    void unregisterMathSimDriver() {
        DriverRegistry::unregisterDriver(MathSimDriver::NAME);
    }

    namespace {
        static const struct MathSimDriverLoader {
            MathSimDriverLoader() {
                registerMathSimDriver();
            }

            ~MathSimDriverLoader() {
                unregisterMathSimDriver();
            }
        } g_driverLoader;

        const char *opKindToString(MathOpKind kind) {
            switch (kind) {
            case MathOpKind::add:
                return "add";
            case MathOpKind::subtract:
                return "subtract";
            case MathOpKind::multiply:
                return "multiply";
            case MathOpKind::pow:
                return "pow";
            case MathOpKind::none:
                return "none";
            }
            return "none";
        }

        MathOpKind opKindFromString(const std::string &value) {
            if (value == "add") {
                return MathOpKind::add;
            }
            if (value == "subtract") {
                return MathOpKind::subtract;
            }
            if (value == "multiply") {
                return MathOpKind::multiply;
            }
            if (value == "pow") {
                return MathOpKind::pow;
            }
            return MathOpKind::none;
        }

        PortDescriptor
        scalarPortDescriptor(PortDirection direction,
                             size_t count,
                             std::vector<std::string> names = {},
                             bool isResizeable = false,
                             std::vector<PortState> defaultStates = {}) {
            return {.direction = direction,
                    .signalKind = SignalKind::scalar,
                    .quantityKind = QuantityKind::dimensionless,
                    .unit = "",
                    .count = count,
                    .names = std::move(names),
                    .isResizeable = isResizeable,
                    .defaultStates = std::move(defaultStates)};
        }

        std::shared_ptr<MathCompDef> loadDef(const Json::Value &defJson) {
            if (!defJson.isObject()) {
                throw std::runtime_error(
                    "math component definition must be an object");
            }

            const auto defName = defJson.get("name", "").asString();
            const auto defTypeName = defJson.get("typeName", "").asString();

            std::shared_ptr<MathCompDef> def;
            if (!defName.empty()) {
                const auto baseDef =
                    ComponentCatalog::instance().getComponentDefinition(
                        defName);
                if (baseDef) {
                    def = std::dynamic_pointer_cast<MathCompDef>(
                        baseDef->clone());
                }
            }

            if (!def && defTypeName == MathCompDef::TypeName) {
                def = std::make_shared<MathCompDef>();
            }

            if (!def) {
                throw std::runtime_error("unknown math component definition '" +
                                         defName + "' (type '" + defTypeName +
                                         "')");
            }

            def->loadJson(defJson);
            def->computeScalarFnIfNeeded();
            return def;
        }

        Connections &connectionsFor(MathSimComp &comp,
                                    PortDirection direction) {
            return direction == PortDirection::input
                       ? comp.getInputConnections()
                       : comp.getOutputConnections();
        }

        const Connections &connectionsFor(const MathSimComp &comp,
                                          PortDirection direction) {
            return direction == PortDirection::input
                       ? comp.getInputConnections()
                       : comp.getOutputConnections();
        }

        std::vector<PortState> &statesFor(MathSimComp &comp,
                                          PortDirection direction) {
            return direction == PortDirection::input ? comp.getInputStates()
                                                     : comp.getOutputStates();
        }

        const std::vector<PortState> &statesFor(const MathSimComp &comp,
                                                PortDirection direction) {
            return direction == PortDirection::input ? comp.getInputStates()
                                                     : comp.getOutputStates();
        }

        std::vector<bool> &connectedFor(MathSimComp &comp,
                                        PortDirection direction) {
            return direction == PortDirection::input
                       ? comp.getIsInputConnected()
                       : comp.getIsOutputConnected();
        }

        PortDirection oppositeDirection(PortDirection direction) {
            if (direction == PortDirection::input) {
                return PortDirection::output;
            }
            if (direction == PortDirection::output) {
                return PortDirection::input;
            }
            return PortDirection::none;
        }

        PortCountChangeRes changeResFor(PortDirection direction) {
            return direction == PortDirection::input
                       ? PortCountChangeRes::inputsChanged()
                       : PortCountChangeRes::outputsChanged();
        }

        bool isSupportedScalarPort(const PortRef &port) {
            return port.isValid() && port.signalKind == SignalKind::scalar &&
                   (port.direction == PortDirection::input ||
                    port.direction == PortDirection::output);
        }

        void markPortConnection(MathSimComp &comp,
                                PortDirection direction,
                                int index,
                                bool connected) {
            auto &connectedList = connectedFor(comp, direction);
            auto &stateList = statesFor(comp, direction);
            if (index < 0 ||
                static_cast<size_t>(index) >= connectedList.size()) {
                return;
            }

            connectedList[index] = connected;
            if (static_cast<size_t>(index) >= stateList.size()) {
                return;
            }

            stateList[index].connState =
                connected ? ConnectionState::driven : ConnectionState::high_z;
        }

        bool stateChanged(const PortState &oldState,
                          const PortState &newState) {
            if (oldState.signalKind != newState.signalKind) {
                return true;
            }

            if (oldState.signalKind != SignalKind::scalar) {
                return oldState != newState;
            }

            if (std::isnan(oldState.scalarValue) &&
                std::isnan(newState.scalarValue)) {
                return false;
            }

            return oldState.scalarValue != newState.scalarValue ||
                   oldState.connState != newState.connState;
        }

        PortDescriptor descriptorFor(const MathCompDef &def,
                                     PortDirection direction) {
            return direction == PortDirection::input
                       ? def.getInputPortDescriptor()
                       : def.getOutputPortDescriptor();
        }

        bool isPortResizeAllowed(const MathCompDef &def,
                                 const PortRef &port,
                                 bool force) {
            if (force) {
                return true;
            }
            return descriptorFor(def, port.direction).isResizeable;
        }
    } // namespace

    MathCompDef::MathCompDef() {
        m_inputPorts = scalarPortDescriptor(PortDirection::input, 0);
        m_outputPorts = scalarPortDescriptor(PortDirection::output, 0);
    }

    std::shared_ptr<MathCompDef>
    MathCompDef::makeBinaryOp(const std::string &name,
                              const std::string &groupName,
                              MathOpKind opKind,
                              TimeNs propDelay) {
        auto def = std::make_shared<MathCompDef>();
        def->setName(name);
        def->setGroupName(groupName);
        def->setInputPortDescriptor(scalarPortDescriptor(
            PortDirection::input, 2, std::vector<std::string>{"A", "B"}));
        def->setOutputPortDescriptor(scalarPortDescriptor(
            PortDirection::output, 1, std::vector<std::string>{"Y"}));
        def->setPropDelay(propDelay);
        def->setOpKind(opKind);
        def->computeScalarFnIfNeeded();
        return def;
    }

    std::shared_ptr<MathCompDef>
    MathCompDef::makeFunction(const std::string &name,
                              const std::string &groupName,
                              const TScalarFn &scalarFn,
                              bool shouldAutoReschedule,
                              TimeNs stepDelay // 10 ms
    ) {
        const auto fnDef = std::make_shared<MathCompDef>();
        fnDef->setName(name);
        fnDef->setGroupName(groupName);
        fnDef->setInputPortDescriptor(
            scalarPortDescriptor(PortDirection::input, 0));
        fnDef->setOutputPortDescriptor(
            scalarPortDescriptor(PortDirection::output,
                                 1,
                                 std::vector<std::string>{
                                     "Y",
                                 }));

        fnDef->setScalarFn(scalarFn);

        fnDef->setAutoReschedule(shouldAutoReschedule);
        fnDef->setAutoRescheduleDelay(stepDelay); // 10 ms
        return fnDef;
    }

    void MathCompDef::setSimFn(const TMathSimFn &simFn) {
        m_simFn = [simFn](const SimFnDataPtr &data) -> SimFnDataPtr {
            auto mathData = std::dynamic_pointer_cast<MathCompSimData>(data);
            if (!mathData) {
                BESS_WARN("(MathCompDef.setSimFn) Invalid data type passed to "
                          "sim function");
                return nullptr;
            }
            return simFn(mathData);
        };
    }

    void MathCompDef::setInputPortDescriptor(const PortDescriptor &descriptor) {
        m_inputPorts = descriptor;
        m_inputPorts.direction = PortDirection::input;
    }

    void
    MathCompDef::setOutputPortDescriptor(const PortDescriptor &descriptor) {
        m_outputPorts = descriptor;
        m_outputPorts.direction = PortDirection::output;
    }

    void MathCompDef::setScalarFn(const TScalarFn &scalarFn) {
        m_scalarFn = scalarFn;
        const auto fn = m_scalarFn;
        setSimFn([fn](const std::shared_ptr<MathCompSimData> &data) {
            if (!data) {
                return data;
            }

            std::vector<double> values;
            values.reserve(data->inputStates.size());

            for (const auto &input : data->inputStates) {
                values.push_back(input.isScalar() ? input.scalarValue : 0.0);
            }

            if (data->outputStates.empty()) {
                data->outputStates.resize(1);
            }

            const auto timeMs =
                std::chrono::duration<double, std::milli>(data->simTime);

            const auto next =
                PortState::scalar(fn ? fn(timeMs, values) : 0.0, data->simTime);
            const auto prev = data->prevState.outputStates.empty()
                                  ? PortState::scalar(0.0)
                                  : data->prevState.outputStates[0];
            data->simDependants = stateChanged(prev, next);
            data->outputStates[0] = next;
            return data;
        });
    }

    PortDescriptor MathCompDef::getInputPortDescriptor() const {
        return m_inputPorts;
    }

    PortDescriptor MathCompDef::getOutputPortDescriptor() const {
        return m_outputPorts;
    }

    Json::Value MathCompDef::toJson() const {
        Json::Value json = EvtBasedCompDef::toJson();
        JsonConvert::toJsonValue(m_inputPorts, json["inputPorts"]);
        JsonConvert::toJsonValue(m_outputPorts, json["outputPorts"]);
        json["opKind"] = opKindToString(m_opKind);
        return json;
    }

    void MathCompDef::loadJson(const Json::Value &json) {
        EvtBasedCompDef::loadJson(json);
        if (json.isMember("inputPorts")) {
            JsonConvert::fromJsonValue(json["inputPorts"], m_inputPorts);
        }
        if (json.isMember("outputPorts")) {
            JsonConvert::fromJsonValue(json["outputPorts"], m_outputPorts);
        }
        if (json.isMember("opKind") && json["opKind"].isString()) {
            m_opKind = opKindFromString(json["opKind"].asString());
        }
        computeScalarFnIfNeeded();
    }

    std::string MathCompDef::getTypeName() const {
        return TypeName;
    }

    std::shared_ptr<CompDef> MathCompDef::clone() const {
        return std::make_shared<MathCompDef>(*this);
    }

    bool MathCompDef::computeScalarFnIfNeeded() {
        switch (m_opKind) {
        case MathOpKind::add:
            setScalarFn([](TimeMs, const std::vector<double> &values) {
                double result = 0.0;
                for (double value : values) {
                    result += value;
                }
                return result;
            });
            return true;
        case MathOpKind::subtract:
            setScalarFn([](TimeMs, const std::vector<double> &values) {
                if (values.empty()) {
                    return 0.0;
                }

                double result = values.front();
                for (size_t i = 1; i < values.size(); ++i) {
                    result -= values[i];
                }
                return result;
            });
            return true;
        case MathOpKind::multiply:
            setScalarFn([](TimeMs, const std::vector<double> &values) {
                if (values.empty()) {
                    return 1.0;
                }

                double result = 1.0;
                for (double value : values) {
                    result *= value;
                }
                return result;
            });
            return true;
        case MathOpKind::pow:
            setScalarFn([](TimeMs, const std::vector<double> &values) {
                if (values.size() < 2) {
                    return 1.0;
                }
                return std::pow(values[0], values[1]);
            });
            return true;
        case MathOpKind::none:
            break;
        }

        return m_scalarFn != nullptr;
    }

    std::shared_ptr<MathSimComp>
    MathSimComp::fromDef(const std::shared_ptr<CompDef> &compDef,
                         bool cloneDef) {
        return fromDef<MathSimComp>(compDef, cloneDef);
    }

    Json::Value MathSimComp::toJson() const {
        Json::Value json = EvtBasedSimComp::toJson();
        JsonConvert::toJsonValue(m_inputStates, json["inputStates"]);
        JsonConvert::toJsonValue(m_outputStates, json["outputStates"]);
        JsonConvert::toJsonValue(m_initialInputStates,
                                 json["initialInputStates"]);
        JsonConvert::toJsonValue(m_initialOutputStates,
                                 json["initialOutputStates"]);
        JsonConvert::toJsonValue(m_inputConnections, json["inputConnections"]);
        JsonConvert::toJsonValue(m_outputConnections,
                                 json["outputConnections"]);
        JsonConvert::toJsonValue(m_isInputConnected, json["isInputConnected"]);
        JsonConvert::toJsonValue(m_isOutputConnected,
                                 json["isOutputConnected"]);
        JsonConvert::toJsonValue(m_netUuid, json["netUuid"]);
        return json;
    }

    void MathSimComp::loadJson(const Json::Value &json) {
        EvtBasedSimComp::loadJson(json);

        if (json.isMember("inputStates")) {
            JsonConvert::fromJsonValue(json["inputStates"], m_inputStates);
        }
        if (json.isMember("outputStates")) {
            JsonConvert::fromJsonValue(json["outputStates"], m_outputStates);
        }
        if (json.isMember("initialInputStates")) {
            JsonConvert::fromJsonValue(json["initialInputStates"],
                                       m_initialInputStates);
        } else {
            // Older scene files only stored the live port state. Treat it as
            // the configured restart state so user-edited values survive the
            // migration to explicit runtime reset state.
            m_initialInputStates = m_inputStates;
        }
        if (json.isMember("initialOutputStates")) {
            JsonConvert::fromJsonValue(json["initialOutputStates"],
                                       m_initialOutputStates);
        } else {
            m_initialOutputStates = m_outputStates;
        }
        if (json.isMember("inputConnections")) {
            JsonConvert::fromJsonValue(json["inputConnections"],
                                       m_inputConnections);
        }
        if (json.isMember("outputConnections")) {
            JsonConvert::fromJsonValue(json["outputConnections"],
                                       m_outputConnections);
        }
        if (json.isMember("isInputConnected")) {
            JsonConvert::fromJsonValue(json["isInputConnected"],
                                       m_isInputConnected);
        }
        if (json.isMember("isOutputConnected")) {
            JsonConvert::fromJsonValue(json["isOutputConnected"],
                                       m_isOutputConnected);
        }
        if (json.isMember("netUuid")) {
            JsonConvert::fromJsonValue(json["netUuid"], m_netUuid);
        }
    }

    void MathSimComp::resetRuntimeState(TimeNs startTime) {
        EvtBasedSimComp::resetRuntimeState(startTime);
        m_inputStates = m_initialInputStates;
        m_outputStates = m_initialOutputStates;
        for (auto &state : m_inputStates) {
            state.lastChangeTime = startTime;
        }
        for (auto &state : m_outputStates) {
            state.lastChangeTime = startTime;
        }
    }

    std::string MathSimDriver::getName() const {
        return MathSimDriver::NAME;
    }

    bool MathSimDriver::simulate(const SimEvt &evt,
                                 const std::vector<PortState> &inputs) {
        const auto comp = getComponent<MathSimComp>(evt.compId);
        if (!comp) {
            BESS_WARN(
                "(MathSimDriver.simulate) Component with UUID {} not found",
                (uint64_t)evt.compId);
            return false;
        }

        auto simData = std::make_shared<MathCompSimData>();
        simData->simTime = getCurrentSimTime();
        simData->prevState.inputStates = comp->getInputStates();
        simData->prevState.outputStates = comp->getOutputStates();
        simData->inputStates = inputs;
        simData->outputStates = comp->getOutputStates();

        auto newData =
            std::dynamic_pointer_cast<MathCompSimData>(comp->simulate(simData));
        if (!newData) {
            BESS_WARN("(MathSimDriver.simulate) Simulation function for "
                      "component with UUID {} did not return MathCompSimData",
                      (uint64_t)evt.compId);
            return false;
        }

        comp->setInputStates(newData->inputStates);
        comp->setOutputStates(newData->outputStates);

        if (newData->simDependants) {
            for (const auto &[_, fn] : comp->getOnStateChangeCbs()) {
                fn(newData->inputStates, newData->outputStates);
            }
        }

        return newData->simDependants;
    }

    UUID MathSimDriver::addComponent(const std::shared_ptr<SimComponent> &comp,
                                     bool scheduleSim) {
        if (!comp) {
            return UUID::null;
        }

        EvtBasedSimDriver::addComponent(comp, scheduleSim);
        return comp->getUuid();
    }

    std::shared_ptr<SimComponent>
    MathSimDriver::createComp(const std::shared_ptr<CompDef> &def,
                              bool cloneDef) {
        if (!supportsDef(def)) {
            BESS_WARN("(MathSimDriver.createComp) Unsupported component "
                      "definition type: {}",
                      def ? def->getName() : "<null>");
            return nullptr;
        }

        const auto comp = MathSimComp::fromDef(def, cloneDef);
        if (!comp) {
            BESS_WARN("(MathSimDriver.createComp) Failed to create component");
            return nullptr;
        }

        BESS_DEBUG(
            "(MathSimDriver.createComp) Created component '{}' with UUID "
            "{} from definition '{}'",
            comp->getName(),
            (uint64_t)comp->getUuid(),
            def->getName());
        return comp;
    }

    void
    MathSimDriver::onComponentAdded(const std::shared_ptr<SimComponent> &comp) {
        const auto mathComp = std::dynamic_pointer_cast<MathSimComp>(comp);
        if (!mathComp) {
            return;
        }

        const auto mathDef = mathComp->getDefinition<MathCompDef>();
        BESS_ASSERT(mathDef,
                    "Component definition for math component with UUID {} is "
                    "not a MathCompDef",
                    (uint64_t)mathComp->getUuid());
        if (mathDef) {
            mathDef->computeScalarFnIfNeeded();
        }

        Net net;
        net.addComponent(mathComp->getUuid());
        mathComp->setNetUuid(net.getUUID());
        m_nets[net.getUUID()] = net;
        m_isNetUpdated = true;
    }

    void MathSimDriver::deleteComponent(const UUID &uuid) {
        const auto comp = getComponent<MathSimComp>(uuid);
        if (!comp) {
            EvtBasedSimDriver::deleteComponent(uuid);
            return;
        }

        auto removeBackReferences = [&](Connections &pins,
                                        PortDirection otherDirection) {
            for (const auto &pin : pins) {
                for (const auto &[otherId, otherIdx] : pin) {
                    const auto other = getComponent<MathSimComp>(otherId);
                    if (!other) {
                        continue;
                    }

                    auto &otherPins = connectionsFor(*other, otherDirection);
                    if (otherIdx < 0 ||
                        static_cast<size_t>(otherIdx) >= otherPins.size()) {
                        continue;
                    }

                    auto &targetPin = otherPins[otherIdx];
                    std::erase_if(targetPin, [&](const auto &conn) {
                        return conn.first == uuid;
                    });
                    markPortConnection(
                        *other, otherDirection, otherIdx, !targetPin.empty());
                }
            }
        };

        removeBackReferences(comp->getOutputConnections(),
                             PortDirection::input);
        removeBackReferences(comp->getInputConnections(),
                             PortDirection::output);

        EvtBasedSimDriver::deleteComponent(uuid);
        rebuildNets();
    }

    void MathSimDriver::clearComponents() {
        EvtBasedSimDriver::clearComponents();
        m_nets.clear();
        m_isNetUpdated = true;
    }

    std::pair<bool, std::string>
    MathSimDriver::canConnectPorts(const PortRef &src,
                                   const PortRef &dst) const {
        if (!isSupportedScalarPort(src) || !isSupportedScalarPort(dst)) {
            return {false, "MathSimDriver only supports scalar ports"};
        }

        if (src.direction == dst.direction) {
            return {false, "Cannot connect ports with the same direction"};
        }

        const auto srcComp = getComponent<MathSimComp>(src.componentId);
        const auto dstComp = getComponent<MathSimComp>(dst.componentId);
        if (!srcComp || !dstComp) {
            return {false,
                    "Source or destination component does not exist in "
                    "MathSimDriver"};
        }

        const auto &srcPins = connectionsFor(*srcComp, src.direction);
        const auto &dstPins = connectionsFor(*dstComp, dst.direction);
        if (src.index < 0 || dst.index < 0 ||
            static_cast<size_t>(src.index) >= srcPins.size() ||
            static_cast<size_t>(dst.index) >= dstPins.size()) {
            return {false, "Invalid port index"};
        }

        const auto &inputComp = src.isInput() ? srcComp : dstComp;
        const auto &inputPort = src.isInput() ? src : dst;
        const auto &inputPins = inputComp->getInputConnections();
        if (static_cast<size_t>(inputPort.index) < inputPins.size() &&
            !inputPins[inputPort.index].empty()) {
            const auto &existing = inputPins[inputPort.index].front();
            const auto &outputPort = src.isOutput() ? src : dst;
            if (existing.first == outputPort.componentId &&
                existing.second == outputPort.index) {
                return {false, "Connection already exists"};
            }
            return {false, "Input port already has a scalar driver"};
        }

        return {true, ""};
    }

    bool MathSimDriver::connectPorts(const PortRef &src,
                                     const PortRef &dst,
                                     bool overrideConn) {
        auto [canConnect, errorMsg] = canConnectPorts(src, dst);
        const auto &inputPort = src.isInput() ? src : dst;
        const auto &outputPort = src.isOutput() ? src : dst;

        if (!canConnect &&
            !(overrideConn &&
              (errorMsg == "Connection already exists" ||
               errorMsg == "Input port already has a scalar driver"))) {
            BESS_WARN("Cannot connect math components: {}", errorMsg);
            return false;
        }

        const auto inputComp = getComponent<MathSimComp>(inputPort.componentId);
        const auto outputComp =
            getComponent<MathSimComp>(outputPort.componentId);
        if (!inputComp || !outputComp) {
            return false;
        }

        auto &inputPins = inputComp->getInputConnections();
        auto &outputPins = outputComp->getOutputConnections();
        if (inputPort.index < 0 || outputPort.index < 0 ||
            static_cast<size_t>(inputPort.index) >= inputPins.size() ||
            static_cast<size_t>(outputPort.index) >= outputPins.size()) {
            return false;
        }

        if (!inputPins[inputPort.index].empty()) {
            const auto oldConnections = inputPins[inputPort.index];
            for (const auto &[oldOutputCompId, oldOutputIdx] : oldConnections) {
                deleteConnection({.componentId = oldOutputCompId,
                                  .direction = PortDirection::output,
                                  .signalKind = SignalKind::scalar,
                                  .index = oldOutputIdx},
                                 inputPort);
            }
        }

        outputPins[outputPort.index].emplace_back(inputPort.componentId,
                                                  inputPort.index);
        inputPins[inputPort.index].emplace_back(outputPort.componentId,
                                                outputPort.index);

        markPortConnection(
            *outputComp, PortDirection::output, outputPort.index, true);
        markPortConnection(
            *inputComp, PortDirection::input, inputPort.index, true);

        if (outputComp->getNetUuid() != inputComp->getNetUuid()) {
            UUID finalNetId = outputComp->getNetUuid();
            UUID movedNetId = inputComp->getNetUuid();

            if (m_nets.contains(finalNetId) && m_nets.contains(movedNetId) &&
                m_nets.at(finalNetId).size() < m_nets.at(movedNetId).size()) {
                std::swap(finalNetId, movedNetId);
            }

            if (m_nets.contains(finalNetId) && m_nets.contains(movedNetId)) {
                auto &finalNet = m_nets[finalNetId];
                auto &movedNet = m_nets[movedNetId];
                for (const auto &compUuid : movedNet.getComponents()) {
                    if (const auto comp = getComponent<MathSimComp>(compUuid)) {
                        comp->setNetUuid(finalNetId);
                    }
                    finalNet.addComponent(compUuid);
                }
                m_nets.erase(movedNetId);
                m_isNetUpdated = true;
            }
        }

        scheduleEvt(inputPort.componentId,
                    getCurrentSimTime(),
                    outputPort.componentId,
                    true);

        BESS_INFO("Connected components in MathSimDriver");
        return true;
    }

    void MathSimDriver::deleteConnection(const PortRef &portA,
                                         const PortRef &portB) {
        const auto compA = getComponent<MathSimComp>(portA.componentId);
        const auto compB = getComponent<MathSimComp>(portB.componentId);
        if (!compA || !compB) {
            return;
        }

        auto &pinsA = connectionsFor(*compA, portA.direction);
        auto &pinsB = connectionsFor(*compB, portB.direction);
        if (portA.index < 0 || portB.index < 0 ||
            static_cast<size_t>(portA.index) >= pinsA.size() ||
            static_cast<size_t>(portB.index) >= pinsB.size()) {
            return;
        }

        auto &portAConnections = pinsA[portA.index];
        auto &portBConnections = pinsB[portB.index];
        const auto removedFromA =
            std::erase_if(portAConnections, [&](const auto &conn) {
                return conn.first == portB.componentId &&
                       conn.second == portB.index;
            });
        const auto removedFromB =
            std::erase_if(portBConnections, [&](const auto &conn) {
                return conn.first == portA.componentId &&
                       conn.second == portA.index;
            });

        if (removedFromA == 0 && removedFromB == 0) {
            return;
        }

        markPortConnection(
            *compA, portA.direction, portA.index, !portAConnections.empty());
        markPortConnection(
            *compB, portB.direction, portB.index, !portBConnections.empty());

        rebuildNets();
        BESS_INFO("Deleted connection in MathSimDriver");
    }

    PortCountChangeRes MathSimDriver::addPort(const PortRef &port, bool force) {
        if (!isSupportedScalarPort(port)) {
            return PortCountChangeRes::noChange();
        }

        const auto comp = getComponent<MathSimComp>(port.componentId);
        if (!comp) {
            return PortCountChangeRes::noChange();
        }

        const auto def = comp->getDefinition<MathCompDef>();
        if (!def || !isPortResizeAllowed(*def, port, force)) {
            return PortCountChangeRes::noChange();
        }

        auto descriptor = descriptorFor(*def, port.direction);
        auto &states = statesFor(*comp, port.direction);
        auto &initialStates = port.direction == PortDirection::input
                                  ? comp->getInitialInputStates()
                                  : comp->getInitialOutputStates();
        auto &connections = connectionsFor(*comp, port.direction);
        auto &connected = connectedFor(*comp, port.direction);
        const auto insertIdx =
            std::clamp(port.index, 0, static_cast<int>(states.size()));

        if (!descriptor.defaultStates.empty()) {
            descriptor.defaultStates.resize(states.size(),
                                            PortState::scalar(0.0));
            descriptor.defaultStates.insert(descriptor.defaultStates.begin() +
                                                insertIdx,
                                            PortState::scalar(0.0));
        }
        initialStates.resize(states.size(), PortState::scalar(0.0));
        states.insert(states.begin() + insertIdx, PortState::scalar(0.0));
        initialStates.insert(initialStates.begin() + insertIdx,
                             PortState::scalar(0.0));
        connections.insert(connections.begin() + insertIdx,
                           Connections::value_type{});
        connected.insert(connected.begin() + insertIdx, false);

        // Inserting a port shifts every following local port index. Keep the
        // reciprocal references stored by connected components in sync.
        for (size_t shiftedIndex = static_cast<size_t>(insertIdx) + 1;
             shiftedIndex < connections.size();
             ++shiftedIndex) {
            for (const auto &[otherId, otherPortIndex] :
                 connections[shiftedIndex]) {
                const auto other = getComponent<MathSimComp>(otherId);
                if (!other || otherPortIndex < 0) {
                    continue;
                }

                auto &otherPins =
                    connectionsFor(*other, oppositeDirection(port.direction));
                if (static_cast<size_t>(otherPortIndex) >= otherPins.size()) {
                    continue;
                }

                for (auto &backReference : otherPins[otherPortIndex]) {
                    if (backReference.first == port.componentId &&
                        backReference.second ==
                            static_cast<int>(shiftedIndex) - 1) {
                        backReference.second = static_cast<int>(shiftedIndex);
                    }
                }
            }
        }
        descriptor.count = states.size();

        if (port.direction == PortDirection::input) {
            def->setInputPortDescriptor(descriptor);
        } else {
            def->setOutputPortDescriptor(descriptor);
        }

        triggerPortCountChangeCbs(port.componentId,
                                  port.direction,
                                  SignalKind::scalar,
                                  states.size());
        return changeResFor(port.direction);
    }

    PortCountChangeRes MathSimDriver::removePort(const PortRef &port,
                                                 bool force) {
        if (!isSupportedScalarPort(port)) {
            return PortCountChangeRes::noChange();
        }

        const auto comp = getComponent<MathSimComp>(port.componentId);
        if (!comp) {
            return PortCountChangeRes::noChange();
        }

        const auto def = comp->getDefinition<MathCompDef>();
        if (!def || !isPortResizeAllowed(*def, port, force)) {
            return PortCountChangeRes::noChange();
        }

        auto &states = statesFor(*comp, port.direction);
        auto &initialStates = port.direction == PortDirection::input
                                  ? comp->getInitialInputStates()
                                  : comp->getInitialOutputStates();
        auto &connections = connectionsFor(*comp, port.direction);
        auto &connected = connectedFor(*comp, port.direction);
        if (port.index < 0 ||
            static_cast<size_t>(port.index) >= states.size()) {
            return PortCountChangeRes::noChange();
        }

        const auto removedConnections = connections[port.index];
        for (const auto &[otherId, otherPortIndex] : removedConnections) {
            deleteConnection(port,
                             {.componentId = otherId,
                              .direction = oppositeDirection(port.direction),
                              .signalKind = SignalKind::scalar,
                              .index = otherPortIndex});
        }

        // The following local ports move down by one. Update their reciprocal
        // references before erasing the local connection slot.
        for (size_t shiftedIndex = static_cast<size_t>(port.index) + 1;
             shiftedIndex < connections.size();
             ++shiftedIndex) {
            for (const auto &[otherId, otherPortIndex] :
                 connections[shiftedIndex]) {
                const auto other = getComponent<MathSimComp>(otherId);
                if (!other || otherPortIndex < 0) {
                    continue;
                }

                auto &otherPins =
                    connectionsFor(*other, oppositeDirection(port.direction));
                if (static_cast<size_t>(otherPortIndex) >= otherPins.size()) {
                    continue;
                }

                for (auto &backReference : otherPins[otherPortIndex]) {
                    if (backReference.first == port.componentId &&
                        backReference.second ==
                            static_cast<int>(shiftedIndex)) {
                        --backReference.second;
                    }
                }
            }
        }

        states.erase(states.begin() + port.index);
        if (static_cast<size_t>(port.index) < initialStates.size()) {
            initialStates.erase(initialStates.begin() + port.index);
        }
        connections.erase(connections.begin() + port.index);
        connected.erase(connected.begin() + port.index);

        auto descriptor = descriptorFor(*def, port.direction);
        if (static_cast<size_t>(port.index) < descriptor.defaultStates.size()) {
            descriptor.defaultStates.erase(descriptor.defaultStates.begin() +
                                           port.index);
        }
        descriptor.count = states.size();
        if (port.direction == PortDirection::input) {
            def->setInputPortDescriptor(descriptor);
        } else {
            def->setOutputPortDescriptor(descriptor);
        }

        triggerPortCountChangeCbs(port.componentId,
                                  port.direction,
                                  SignalKind::scalar,
                                  states.size());
        return changeResFor(port.direction);
    }

    ConnectionBundle MathSimDriver::getConnections(const UUID &uuid) const {
        ConnectionBundle bundle;
        const auto comp = getComponent<MathSimComp>(uuid);
        if (!comp) {
            return bundle;
        }

        bundle.inputs = comp->getInputConnections();
        bundle.outputs = comp->getOutputConnections();
        return bundle;
    }

    std::vector<UUID> MathSimDriver::getDependants(const UUID &id) {
        const auto comp = getComponent<MathSimComp>(id);
        if (!comp) {
            return {};
        }

        std::vector<UUID> dependants;
        for (const auto &pinConnections : comp->getOutputConnections()) {
            for (const auto &[targetId, _] : pinConnections) {
                if (targetId != UUID::null &&
                    std::ranges::find(dependants, targetId) ==
                        dependants.end()) {
                    dependants.emplace_back(targetId);
                }
            }
        }
        return dependants;
    }

    std::vector<PortState> MathSimDriver::collapseInputs(const UUID &id) {
        const auto comp = getComponent<MathSimComp>(id);
        if (!comp) {
            return {};
        }

        auto collapsed = comp->getInputStates();
        const auto &inputConns = comp->getInputConnections();
        if (collapsed.size() < inputConns.size()) {
            collapsed.resize(inputConns.size(), PortState::scalar(0.0));
        }

        for (size_t pinIdx = 0; pinIdx < inputConns.size(); ++pinIdx) {
            const auto &pinConns = inputConns[pinIdx];
            if (pinConns.empty()) {
                if (!collapsed[pinIdx].isScalar()) {
                    collapsed[pinIdx] =
                        PortState::scalar(0.0, getCurrentSimTime());
                }
                collapsed[pinIdx].connState = ConnectionState::high_z;
                continue;
            }

            const auto &[srcId, srcSlotIdx] = pinConns.front();
            const auto srcComp = getComponent<MathSimComp>(srcId);
            if (!srcComp || srcSlotIdx < 0 ||
                static_cast<size_t>(srcSlotIdx) >=
                    srcComp->getOutputStates().size()) {
                collapsed[pinIdx] = PortState::scalar(0.0, getCurrentSimTime());
                collapsed[pinIdx].connState = ConnectionState::unknown;
                continue;
            }

            const auto &srcState = srcComp->getOutputStates()[srcSlotIdx];
            collapsed[pinIdx] =
                srcState.isScalar() ? srcState : PortState::scalar(0.0);
        }

        return collapsed;
    }

    std::vector<PortState>
    MathSimDriver::getInputPortStates(const UUID &compId) {
        return collapseInputs(compId);
    }

    PortState MathSimDriver::getPortState(const PortRef &port) const {
        const auto comp = getComponent<MathSimComp>(port.componentId);
        if (!comp || !isSupportedScalarPort(port) || port.index < 0) {
            return PortState::scalar(0.0);
        }

        const auto &states = statesFor(*comp, port.direction);
        if (static_cast<size_t>(port.index) >= states.size()) {
            return PortState::scalar(0.0);
        }

        return states[port.index];
    }

    bool MathSimDriver::setInputPortState(const UUID &uuid,
                                          int pinIdx,
                                          const PortState &state) {
        const auto comp = getComponent<MathSimComp>(uuid);
        if (!comp || !state.isScalar()) {
            return false;
        }

        auto &inputs = comp->getInputStates();
        if (pinIdx < 0 || static_cast<size_t>(pinIdx) >= inputs.size()) {
            return false;
        }

        const auto prev = inputs[pinIdx];
        inputs[pinIdx] = state;
        inputs[pinIdx].lastChangeTime = getCurrentSimTime();
        if (static_cast<size_t>(pinIdx) < comp->getIsInputConnected().size() &&
            !comp->getIsInputConnected()[pinIdx]) {
            inputs[pinIdx].connState = ConnectionState::high_z;
        }

        if (!isRunning() && !isPaused()) {
            auto &initialInputs = comp->getInitialInputStates();
            if (static_cast<size_t>(pinIdx) < initialInputs.size()) {
                initialInputs[pinIdx] = inputs[pinIdx];
            }
        }

        if (prev != inputs[pinIdx]) {
            scheduleEvt(uuid, getCurrentSimTime(), uuid);
        }
        return true;
    }

    bool MathSimDriver::setOutputPortState(const UUID &uuid,
                                           int pinIdx,
                                           const PortState &state) {
        const auto comp = getComponent<MathSimComp>(uuid);
        if (!comp || !state.isScalar()) {
            return false;
        }

        auto &outputs = comp->getOutputStates();
        if (pinIdx < 0 || static_cast<size_t>(pinIdx) >= outputs.size()) {
            return false;
        }

        outputs[pinIdx] = state;
        outputs[pinIdx].lastChangeTime = getCurrentSimTime();
        if (!isRunning() && !isPaused()) {
            auto &initialOutputs = comp->getInitialOutputStates();
            if (static_cast<size_t>(pinIdx) < initialOutputs.size()) {
                initialOutputs[pinIdx] = outputs[pinIdx];
            }
        }
        propagateFromComponent(uuid);
        return true;
    }

    ComponentState MathSimDriver::getComponentState(const UUID &uuid) const {
        ComponentState state;
        const auto comp = getComponent<MathSimComp>(uuid);
        if (!comp) {
            return state;
        }

        state.inputStates = comp->getInputStates();
        state.outputStates = comp->getOutputStates();
        state.inputConnected = comp->getIsInputConnected();
        state.outputConnected = comp->getIsOutputConnected();
        return state;
    }

    const std::unordered_map<UUID, Net> &MathSimDriver::getNetsMap() const {
        return m_nets;
    }

    bool MathSimDriver::isNetUpdated() const {
        return m_isNetUpdated;
    }

    void MathSimDriver::clearNetUpdated() {
        m_isNetUpdated = false;
    }

    void MathSimDriver::rebuildNets() {
        rebuildComponentNets<MathSimComp>(m_components, m_nets);
        m_isNetUpdated = true;
    }

    Json::Value MathSimDriver::toJson() const {
        Json::Value json = Json::Value(Json::objectValue);
        for (const auto &[id, comp] : m_components) {
            json["components"].append(comp->toJson());
        }
        return json;
    }

    void MathSimDriver::loadJson(const Json::Value &json) {
        clearPendingEvents();
        clearComponents();

        if (!json.isObject() || !json.isMember("components") ||
            !json["components"].isArray()) {
            m_isNetUpdated = false;
            return;
        }

        {
            std::lock_guard lk(m_compMapMutex);
            for (const auto &compJson : json["components"]) {
                if (!compJson.isObject() || !compJson.isMember("def")) {
                    continue;
                }

                const auto def = loadDef(compJson["def"]);
                const auto comp = std::dynamic_pointer_cast<MathSimComp>(
                    createComp(def, false));
                if (!comp) {
                    continue;
                }

                comp->loadJson(compJson);
                m_components[comp->getUuid()] = comp;
            }
        }

        registerLoadedComponentsForRunStart();

        rebuildNets();

        m_isNetUpdated = false;
    }

    void MathSimDriver::onInit() {
        auto &catalog = ComponentCatalog::instance();
        catalog.registerComponent(
            MathCompDef::makeBinaryOp("Add", "Maths", MathOpKind::add));

        catalog.registerComponent(MathCompDef::makeBinaryOp(
            "Subtract", "Maths", MathOpKind::subtract));

        catalog.registerComponent(MathCompDef::makeBinaryOp(
            "Multiply", "Maths", MathOpKind::multiply));

        catalog.registerComponent(
            MathCompDef::makeBinaryOp("Power (a^b)", "Maths", MathOpKind::pow));

        const auto inpDef = std::make_shared<MathCompDef>();
        inpDef->setName("Scalar Input");
        inpDef->setGroupName("IO");

        inpDef->setSimFn([](const std::shared_ptr<MathCompSimData> &data) {
            data->simDependants = true;
            return data;
        });

        inpDef->setBehaviorType(ComponentBehaviorType::input);
        inpDef->setInputPortDescriptor(
            scalarPortDescriptor(PortDirection::input, 0));
        inpDef->setOutputPortDescriptor(
            scalarPortDescriptor(PortDirection::output,
                                 1,
                                 std::vector<std::string>{
                                     "Y",
                                 },
                                 true));

        catalog.registerComponent(inpDef);

        const auto outDef = std::make_shared<MathCompDef>();
        outDef->setName("Scalar Output");
        outDef->setGroupName("IO");
        outDef->setBehaviorType(ComponentBehaviorType::output);
        outDef->setInputPortDescriptor(
            scalarPortDescriptor(PortDirection::input,
                                 1,
                                 std::vector<std::string>{
                                     "A",
                                 },
                                 true));
        outDef->setOutputPortDescriptor(
            scalarPortDescriptor(PortDirection::output, 0));

        outDef->setSimFn([](const std::shared_ptr<MathCompSimData> &data) {
            data->simDependants = true;
            return data;
        });

        catalog.registerComponent(outDef);

        auto fnDef = MathCompDef::makeFunction(
            "Sine (sin((f*t) + p) * a)",
            "Maths",
            [](TimeMs time, const std::vector<double> &values) {
                BESS_ASSERT(values.size() == 3,
                            "Expected 3 values for sine function: frequency, "
                            "phase, amplitude");
                const auto freq = values[0];
                const auto phase = values[1];
                const auto amp = values[2];
                const auto t = time.count() / 1000.0;
                return std::sin((t * freq) + phase) * amp;
            });

        fnDef->setInputPortDescriptor(
            scalarPortDescriptor(PortDirection::input,
                                 3,
                                 std::vector<std::string>{
                                     "Frequency (f)",
                                     "Phase (p)",
                                     "Amplitude (a)",
                                 },
                                 false,
                                 {PortState::scalar(1.0),
                                  PortState::scalar(0.0),
                                  PortState::scalar(1.0)}));

        catalog.registerComponent(fnDef);

        fnDef = MathCompDef::makeFunction(
            "Cosine (cos((f*t) + p) * a)",
            "Maths",
            [](TimeMs time, const std::vector<double> &values) {
                BESS_ASSERT(values.size() == 3,
                            "Expected 3 values for sine function: frequency, "
                            "phase, amplitude");
                const auto freq = values[0];
                const auto phase = values[1];
                const auto amp = values[2];
                const auto t = time.count() / 1000.0;
                return std::cos((t * freq) + phase) * amp;
            });

        fnDef->setInputPortDescriptor(
            scalarPortDescriptor(PortDirection::input,
                                 3,
                                 std::vector<std::string>{
                                     "Frequency (f)",
                                     "Phase (p)",
                                     "Amplitude (a)",
                                 },
                                 false,
                                 {PortState::scalar(1.0),
                                  PortState::scalar(0.0),
                                  PortState::scalar(1.0)}));

        catalog.registerComponent(fnDef);

        fnDef = MathCompDef::makeFunction(
            "Exp Decay (e^-xt)",
            "Maths",
            [](TimeMs time, const std::vector<double> &values) {
                auto x = values.empty() ? 1.0 : values[0];
                const auto t = time.count() / 1000.0;
                return std::exp(-x * t);
            });

        fnDef->setInputPortDescriptor(
            scalarPortDescriptor(PortDirection::input,
                                 1,
                                 std::vector<std::string>{
                                     "X",
                                 },
                                 false,
                                 {PortState::scalar(1.0)}));

        catalog.registerComponent(fnDef);

        // auto schedule is FALSE for nodes "out of time"
        // p.s. Proud of quote out of time

        fnDef = MathCompDef::makeFunction(
            "Logarithm (log(x))",
            "Maths",
            [](TimeMs, const std::vector<double> &values) {
                auto x = values.empty() ? 1.0 : values[0];
                return std::log(x);
            });

        fnDef->setInputPortDescriptor(
            scalarPortDescriptor(PortDirection::input,
                                 1,
                                 std::vector<std::string>{
                                     "X",
                                 },
                                 false,
                                 {PortState::scalar(1.0)}));
        fnDef->setAutoReschedule(false);

        catalog.registerComponent(fnDef);

        fnDef = MathCompDef::makeFunction(
            "Square Root (sqrt(x))",
            "Maths",
            [](TimeMs, const std::vector<double> &values) {
                auto x = values.empty() ? 1.0 : values[0];
                return std::sqrt(x);
            });

        fnDef->setInputPortDescriptor(
            scalarPortDescriptor(PortDirection::input,
                                 1,
                                 std::vector<std::string>{
                                     "X",
                                 },
                                 false,
                                 {PortState::scalar(1.0)}));
        fnDef->setAutoReschedule(false);

        catalog.registerComponent(fnDef);

        fnDef = MathCompDef::makeFunction(
            "Absolute Value (abs(x))",
            "Maths",
            [](TimeMs, const std::vector<double> &values) {
                auto x = values.empty() ? 0.0 : values[0];
                return std::abs(x);
            });

        fnDef->setInputPortDescriptor(
            scalarPortDescriptor(PortDirection::input,
                                 1,
                                 std::vector<std::string>{
                                     "X",
                                 }));

        fnDef->setAutoReschedule(false);

        catalog.registerComponent(fnDef);

        fnDef = MathCompDef::makeFunction(
            "Clamp (clamp(x, min, max))",
            "Maths",
            [](TimeMs, const std::vector<double> &values) {
                BESS_ASSERT(
                    values.size() == 3,
                    "Expected 3 values for clamp function: x, min, max");
                const auto x = values[0];
                const auto min = values[1];
                const auto max = values[2];
                return std::clamp(x, min, max);
            });

        fnDef->setInputPortDescriptor(
            scalarPortDescriptor(PortDirection::input,
                                 3,
                                 std::vector<std::string>{
                                     "X",
                                     "Min",
                                     "Max",
                                 }));

        fnDef->setAutoReschedule(false);

        catalog.registerComponent(fnDef);

        fnDef = MathCompDef::makeFunction(
            "Modulo (mod(x, y))",
            "Maths",
            [](TimeMs, const std::vector<double> &values) {
                BESS_ASSERT(values.size() == 2,
                            "Expected 2 values for modulo function: x, y");
                const auto x = values[0];
                const auto y = values[1];
                return std::fmod(x, y);
            });

        fnDef->setInputPortDescriptor(scalarPortDescriptor(
            PortDirection::input,
            2,
            std::vector<std::string>{
                "X",
                "Y",
            },
            false,
            {PortState::scalar(0.0), PortState::scalar(1.0)}));

        fnDef->setAutoReschedule(false);

        catalog.registerComponent(fnDef);

        // TimeNode - Just passes time out in seconds and ms

        auto timeNode = std::make_shared<MathCompDef>();
        timeNode->setName("Time Node");
        timeNode->setGroupName("Maths");
        timeNode->setBehaviorType(ComponentBehaviorType::input);
        timeNode->setInputPortDescriptor(
            scalarPortDescriptor(PortDirection::input, 0));
        timeNode->setOutputPortDescriptor(
            scalarPortDescriptor(PortDirection::output,
                                 2,
                                 std::vector<std::string>{
                                     "Seconds",
                                     "Milliseconds",
                                 }));
        timeNode->setSimFn([](const std::shared_ptr<MathCompSimData> &data) {
            const auto timeMs =
                std::chrono::duration<double, std::milli>(data->simTime);
            const auto t = timeMs.count();
            data->outputStates[0] =
                PortState::scalar(t / 1000.0, data->simTime);
            data->outputStates[1] = PortState::scalar(t, data->simTime);
            data->simDependants = true;
            return data;
        });

        timeNode->setAutoReschedule(true);
        timeNode->setAutoRescheduleDelay(TimeNs(2e6));
        catalog.registerComponent(timeNode);

        fnDef = MathCompDef::makeFunction(
            "Diffrentiate x^2",
            "Maths",
            [](TimeMs, const std::vector<double> &values) {
                const auto x = values[0];
                return 2 * x;
                // Equation x("x");
                // Equation y = sin(x) + cos(x) + pow(x, 2);
                // auto y1 = y.derivative(x);
                //
                // std::cout << "Derivative: " << y1 << std::endl;
            });

        fnDef->setInputPortDescriptor(
            scalarPortDescriptor(PortDirection::input,
                                 1,
                                 std::vector<std::string>{
                                     "X",
                                 },
                                 false,
                                 {PortState::scalar(0.0)}));

        fnDef->setAutoReschedule(false);

        catalog.registerComponent(fnDef);
    }

} // namespace Bess::SimEngine::Drivers::Math

namespace Bess::JsonConvert {
    void toJsonValue(Json::Value &json,
                     const Bess::SimEngine::Drivers::Math::MathSimComp &data) {
        json = data.toJson();
    }
} // namespace Bess::JsonConvert
