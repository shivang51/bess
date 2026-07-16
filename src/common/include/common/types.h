#pragma once

#include "common/bess_api.h"

#include "common/bess_uuid.h"
#include <any>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <limits>
#include <ratio>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/container/linked_hash_set.h"
#include "absl/container/node_hash_map.h"

namespace Bess {
    using TimeMs = std::chrono::duration<double, std::milli>;
    using TimeNs = std::chrono::duration<double, std::nano>;

    template <typename K, typename V> using HashMap = absl::flat_hash_map<K, V>;
    template <typename K, typename V>
    using NodeHashMap = absl::node_hash_map<K, V>;
    template <typename K> using HashSet = absl::flat_hash_set<K>;
    template <typename K> using OrderedSet = absl::linked_hash_set<K>;

    struct BESS_API PickingId {
        uint32_t runtimeId;
        uint32_t info;

        struct InfoFlags {
            static constexpr uint32_t unSelectable = 1 << 31;
            static constexpr uint32_t passiveCursor = 1 << 29;
        };

        static constexpr uint32_t invalidRuntimeId =
            std::numeric_limits<uint32_t>::max();

        static constexpr PickingId invalid() noexcept {
            return {invalidRuntimeId, 0};
        }

        constexpr bool
        operator==(const PickingId &other) const noexcept = default;

        constexpr bool isValid() const noexcept {
            return runtimeId != invalidRuntimeId;
        }

        constexpr bool isSelectable() const noexcept {
            return !(info & InfoFlags::unSelectable);
        }

        constexpr uint64_t toUint64() const noexcept {
            return (static_cast<uint64_t>(runtimeId) << 32) |
                   static_cast<uint64_t>(info);
        }

        constexpr operator uint64_t() const noexcept {
            return toUint64();
        }

        static constexpr PickingId fromUint64(uint64_t value) noexcept {
            return {static_cast<uint32_t>(value >> 32),
                    static_cast<uint32_t>(value & 0xFFFFFFFF)};
        }

        // Only use it for widget without node or component parent,
        // Or when runtime id is not available. This will create picking id with
        // runtimeId = 0. As scene state makes sure runtime id 0 is not
        // assigned to a scene component.
        //
        // NOTE: Make sure info is unique for each unique widget
        static constexpr PickingId forWidget(uint32_t info) {
            return {0, info};
        }

        void set(uint64_t value) {
            runtimeId = static_cast<uint32_t>(value >> 32);
            info = static_cast<uint32_t>(value & 0xFFFFFFFF);
        }
    };

    namespace SimEngine {
        typedef TimeNs SimTime;
        typedef std::chrono::duration<double> SimDelaySeconds;
        typedef TimeNs SimDelayNanoSeconds;

        typedef std::pair<UUID, int> ComponentPin;

        typedef std::vector<std::vector<ComponentPin>> Connections;

        struct BESS_API ConnectionBundle {
            Connections inputs;
            Connections outputs;
        };

        enum class SimulationState : uint8_t { running, paused, stopped };

        enum class LogicState : uint8_t { low, high, unknown, high_z };

        enum class SlotsGroupType : uint8_t { none, input, output };

        enum class SlotCatergory : uint8_t {
            none,
            clock,
            clear,
            enable,
        };

        enum class ComponentBehaviorType : uint8_t { none, input, output };

        enum class PortDirection : uint8_t { none, input, output };

        enum class SignalKind : uint8_t { none, digital, scalar, vector };

        enum class QuantityKind : uint8_t {
            none,
            logic,
            dimensionless,
            voltage,
            current,
            resistance,
            conductance,
            power,
            frequency,
            angle,
            time,
            temperature
        };

        struct BESS_API PortRef {
            UUID componentId = UUID::null;
            PortDirection direction = PortDirection::none;
            SignalKind signalKind = SignalKind::none;
            int index = -1;

            bool isValid() const {
                return componentId != UUID::null &&
                       direction != PortDirection::none &&
                       signalKind != SignalKind::none && index >= 0;
            }

            bool isInput() const {
                return direction == PortDirection::input;
            }

            bool isOutput() const {
                return direction == PortDirection::output;
            }
        };

        struct BESS_API PortDescriptor {
            PortDirection direction = PortDirection::none;
            SignalKind signalKind = SignalKind::none;
            QuantityKind quantityKind = QuantityKind::none;
            std::string unit;
            size_t count = 0;
            std::vector<std::string> names;
            bool isResizeable = false;
        };

        enum class ConnectionState : uint8_t { unknown = 0, driven, high_z };

        struct BESS_API LogicThresholds {
            float highThreshold = 0.8f;
            float lowThreshold = 2.0f;
        };

        struct BESS_API PortState {
            SignalKind signalKind = SignalKind::digital;
            LogicState state = LogicState::low;
            double scalarValue = 0.0;
            std::vector<double> vectorValue;
            SimTime lastChangeTime{0};
            ConnectionState connState = ConnectionState::driven;

            PortState() noexcept = default;

            explicit PortState(double value, SimTime time) noexcept
                : signalKind(SignalKind::scalar),
                  scalarValue(value),
                  lastChangeTime(time) {
            }

            PortState(LogicState logicState,
                      SimTime time,
                      const LogicThresholds &thresholds = {}) noexcept
                : signalKind(SignalKind::digital),
                  lastChangeTime(time) {
                fromLogicState(logicState, thresholds);
            }

            static PortState scalar(double value, SimTime time = SimTime{0}) {
                PortState state;
                state.signalKind = SignalKind::scalar;
                state.scalarValue = value;
                state.lastChangeTime = time;
                return state;
            }

            static PortState digital(LogicState value,
                                     SimTime time = SimTime{0}) {
                return PortState{value, time};
            }

            static PortState vector(std::vector<double> value,
                                    SimTime time = SimTime{0}) {
                PortState state;
                state.signalKind = SignalKind::vector;
                state.vectorValue = std::move(value);
                state.lastChangeTime = time;
                return state;
            }

            bool isDigital() const noexcept {
                return signalKind == SignalKind::digital;
            }

            bool isScalar() const noexcept {
                return signalKind == SignalKind::scalar;
            }

            bool isVector() const noexcept {
                return signalKind == SignalKind::vector;
            }

            PortState &setScalarValue(double value,
                                      SimTime time = SimTime{0}) noexcept {
                signalKind = SignalKind::scalar;
                scalarValue = value;
                lastChangeTime = time;
                connState = ConnectionState::driven;
                state = LogicState::unknown;
                return *this;
            }

            PortState &setVectorValue(std::vector<double> value,
                                      SimTime time = SimTime{0}) {
                signalKind = SignalKind::vector;
                vectorValue = std::move(value);
                lastChangeTime = time;
                connState = ConnectionState::driven;
                state = LogicState::unknown;
                return *this;
            }

            bool operator==(const PortState &other) const {
                if (signalKind != other.signalKind) {
                    return false;
                }

                switch (signalKind) {
                case SignalKind::scalar:
                    return scalarValue == other.scalarValue;
                case SignalKind::vector:
                    return vectorValue == other.vectorValue;
                case SignalKind::digital:
                case SignalKind::none:
                    return getLogicState() == other.getLogicState() &&
                           connState == other.connState;
                }

                return false;
            }

            bool operator!=(const PortState &other) const {
                return !(*this == other);
            }

            LogicState getLogicState(
                const LogicThresholds &thresholds = {}) const noexcept {
                (void)thresholds;
                if (signalKind != SignalKind::digital &&
                    signalKind != SignalKind::none) {
                    return LogicState::unknown;
                }

                if (connState == ConnectionState::high_z ||
                    state == LogicState::high_z)
                    return LogicState::high_z;

                if (connState == ConnectionState::unknown ||
                    state == LogicState::unknown)
                    return LogicState::unknown;

                return state;
            }

            PortState &operator=(const LogicState &logicState) noexcept {
                signalKind = SignalKind::digital;
                fromLogicState(logicState, {});
                return *this;
            }

            bool isHighZ() const noexcept {
                return getLogicState() == LogicState::high_z;
            }

            bool isUnknown() const noexcept {
                return getLogicState() == LogicState::unknown;
            }

            bool isHigh(const LogicThresholds &thresholds = {}) const noexcept {
                return getLogicState(thresholds) == LogicState::high;
            }

            bool isLow(const LogicThresholds &thresholds = {}) const noexcept {
                return getLogicState(thresholds) == LogicState::low;
            }

            bool operator==(const LogicState &other) const noexcept {
                return getLogicState({}) == other;
            }

            double getDigitalVoltageValue() const noexcept {
                switch (getLogicState()) {
                case LogicState::low:
                    return 0.0;
                case LogicState::high:
                    return 5.0;
                case LogicState::high_z:
                case LogicState::unknown:
                    return std::numeric_limits<double>::quiet_NaN();
                }
                return std::numeric_limits<double>::quiet_NaN();
            }

            double getNumericValue() const noexcept {
                if (signalKind == SignalKind::scalar) {
                    return scalarValue;
                }
                if (signalKind == SignalKind::digital) {
                    return getDigitalVoltageValue();
                }
                return std::numeric_limits<double>::quiet_NaN();
            }

          private:
            void fromLogicState(LogicState logicState,
                                const LogicThresholds &thresholds) noexcept {
                (void)thresholds;
                state = logicState;
                signalKind = SignalKind::digital;
                scalarValue = 0.0;
                vectorValue.clear();

                switch (logicState) {
                case LogicState::low:
                    scalarValue = 0.0;
                    connState = ConnectionState::driven;
                    break;
                case LogicState::high:
                    connState = ConnectionState::driven;
                    break;
                case LogicState::high_z:
                    scalarValue = 0.0;
                    connState = ConnectionState::high_z;
                    break;
                case LogicState::unknown:
                    scalarValue = 0.0;
                    connState = ConnectionState::unknown;
                    break;
                }
            }
        };

        struct BESS_API SimulationEvent {
            SimTime simTime;
            UUID compId;
            UUID schedulerId; // enitity that triggered the change
            uint64_t id;
            bool operator<(const SimulationEvent &other) const noexcept {
                if (simTime != other.simTime)
                    return simTime < other.simTime;
                return id < other.id;
            }
        };

        struct BESS_API SlotsGroupInfo {
            SlotsGroupType type = SlotsGroupType::none;
            bool isResizeable = false;
            size_t count = 0;
            std::vector<std::string> names;
            std::vector<std::pair<int, SlotCatergory>>
                categories; // slot_index, category
        };

        struct BESS_API OperatorInfo {
            char op = '0';
            bool shouldNegateOutput = false;
        };

        struct BESS_API ComponentState {
            std::vector<PortState> inputStates;
            std::vector<bool> inputConnected;
            std::vector<PortState> outputStates;
            std::vector<bool> outputConnected;
            bool isChanged = false;
            bool simError = false;
            std::any *auxData = nullptr;
            std::string errorMessage;
        };

        typedef std::function<ComponentState(
            const std::vector<PortState> &, SimTime, const ComponentState &)>
            SimulationFunction;

    } // namespace SimEngine
}; // namespace Bess

namespace Bess::JsonConvert {

    template <typename T>
    void fromJsonValue(const Json::Value &j, Bess::HashSet<T> &vec) {
        vec.clear();
        if (j.isArray()) {
            for (const auto &item : j) {
                T val;
                fromJsonValue(item, val);
                vec.insert(val);
            }
        }
    }

    template <typename T>
    void toJsonValue(const Bess::HashSet<T> &vec, Json::Value &j) {
        j = Json::arrayValue;
        for (const auto &item : vec) {
            Json::Value val;
            toJsonValue(item, val);
            j.append(val);
        }
    }

    template <typename T>
    void fromJsonValue(const Json::Value &j, Bess::OrderedSet<T> &vec) {
        vec.clear();
        if (j.isArray()) {
            for (const auto &item : j) {
                T val;
                fromJsonValue(item, val);
                vec.insert(val);
            }
        }
    }

    template <typename T>
    void toJsonValue(const Bess::OrderedSet<T> &vec, Json::Value &j) {
        j = Json::arrayValue;
        for (const auto &item : vec) {
            Json::Value val;
            toJsonValue(item, val);
            j.append(val);
        }
    }

    template <typename K, typename V>
    void fromJsonValue(const Json::Value &j, Bess::HashMap<K, V> &map) {
        map.clear();
        if (j.isObject()) {
            for (const auto &key : j.getMemberNames()) {
                K k;
                fromJsonValue(Json::Value(key), k);
                V v;
                fromJsonValue(j[key], v);
                map[k] = v;
            }
        }
    }

    template <typename K, typename V>
    void toJsonValue(const Bess::HashMap<K, V> &map, Json::Value &j) {
        j = Json::objectValue;
        for (const auto &[key, value] : map) {
            Json::Value k;
            toJsonValue(key, k);
            Json::Value v;
            toJsonValue(value, v);
            j[k.asString()] = v;
        }
    }
} // namespace Bess::JsonConvert

REFLECT_ENUM(Bess::SimEngine::SimulationState)
REFLECT_ENUM(Bess::SimEngine::LogicState)
REFLECT_ENUM(Bess::SimEngine::ConnectionState)
REFLECT_ENUM(Bess::SimEngine::SlotsGroupType)
REFLECT_ENUM(Bess::SimEngine::SlotCatergory)
REFLECT_ENUM(Bess::SimEngine::ComponentBehaviorType)
REFLECT_ENUM(Bess::SimEngine::PortDirection)
REFLECT_ENUM(Bess::SimEngine::SignalKind)
REFLECT_ENUM(Bess::SimEngine::QuantityKind)

REFLECT(Bess::SimEngine::PortRef, componentId, direction, signalKind, index)
REFLECT(Bess::SimEngine::PortDescriptor,
        direction,
        signalKind,
        quantityKind,
        unit,
        count,
        names,
        isResizeable)

REFLECT(Bess::SimEngine::PortState,
        signalKind,
        state,
        scalarValue,
        vectorValue,
        lastChangeTime,
        connState)
REFLECT_VECTOR(Bess::SimEngine::PortState)
REFLECT_VECTOR(bool)

REFLECT(Bess::SimEngine::ComponentState,
        inputStates,
        inputConnected,
        outputStates,
        outputConnected,
        isChanged,
        simError,
        errorMessage)

REFLECT_VECTOR(Bess::SimEngine::ComponentState)

REFLECT(Bess::SimEngine::ComponentPin, first, second)
REFLECT_VECTOR(Bess::SimEngine::ComponentPin)
REFLECT_VECTOR(std::vector<Bess::SimEngine::ComponentPin>)

REFLECT(SimEngine::OperatorInfo, op, shouldNegateOutput)

typedef std::pair<int, Bess::SimEngine::SlotCatergory> SlotCategoryPair;
REFLECT(SlotCategoryPair, first, second)
REFLECT_VECTOR(SlotCategoryPair)
REFLECT(Bess::SimEngine::SlotsGroupInfo,
        type,
        isResizeable,
        count,
        names,
        categories)
