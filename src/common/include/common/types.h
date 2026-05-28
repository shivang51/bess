#pragma once

#include "common/bess_uuid.h"
#include <any>
#include <chrono>
#include <ratio>

namespace Bess {
    using TimeMs = std::chrono::duration<double, std::milli>;
    using TimeNs = std::chrono::duration<double, std::nano>;

    struct PickingId {
        uint32_t runtimeId;
        uint32_t info;

        struct InfoFlags {
            static constexpr uint32_t unSelectable = 1 << 31;
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

        constexpr operator uint64_t() const noexcept { return toUint64(); }

        static constexpr PickingId fromUint64(uint64_t value) noexcept {
            return {static_cast<uint32_t>(value >> 32),
                    static_cast<uint32_t>(value & 0xFFFFFFFF)};
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

        struct ConnectionBundle {
            Connections inputs;
            Connections outputs;
        };

        enum class SimulationState : uint8_t { running, paused };

        enum class LogicState : uint8_t { low, high, unknown, high_z };

        enum class SlotsGroupType : uint8_t { none, input, output };

        enum class SlotCatergory : uint8_t {
            none,
            clock,
            clear,
            enable,
        };

        enum class ComponentBehaviorType : uint8_t { none, input, output };

        enum class SlotType : uint8_t { digitalInput, digitalOutput };

        enum class ConnectionState : uint8_t { unknown = 0, driven, high_z };

        struct LogicThresholds {
            float highThreshold = 0.8f;
            float lowThreshold = 2.0f;
        };

        struct SlotState {
            float voltage = 0.0f;
            SimTime lastChangeTime{0};
            ConnectionState connState = ConnectionState::high_z;

            constexpr SlotState() noexcept = default;

            constexpr SlotState(float voltage, SimTime time) noexcept
                : voltage(voltage),
                  lastChangeTime(time) {}

            constexpr SlotState(LogicState logicState, SimTime time,
                                const LogicThresholds &thresholds = {}) noexcept
                : lastChangeTime(time) {
                auto [volt, conn] = fromLogicState(logicState, thresholds);
                voltage = volt;
                connState = conn;
            }

            bool operator==(const SlotState &other) const noexcept {
                return voltage == other.voltage;
            }

            bool operator!=(const SlotState &other) const noexcept {
                return !(*this == other);
            }

            // Returns the digital logic state based on the voltage and given
            // thresholds
            LogicState getLogicState(
                const LogicThresholds &thresholds = {}) const noexcept {
                if (connState == ConnectionState::high_z)
                    return LogicState::high_z;
                else if (voltage > thresholds.highThreshold)
                    return LogicState::high;
                else if (voltage < thresholds.lowThreshold)
                    return LogicState::low;
                else
                    return LogicState::unknown;
            }

            SlotState &operator=(const LogicState &state) noexcept {
                auto [volt, conn] = fromLogicState(state, {});
                voltage = volt;
                connState = conn;
                return *this;
            }

            bool isHighZ() const noexcept { return std::isnan(voltage); }

            bool isUnknown() const noexcept {
                return !isHighZ() && std::isnan(voltage);
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

          private:
            static constexpr std::pair<float, ConnectionState>
            fromLogicState(LogicState state,
                           const LogicThresholds &thresholds) noexcept {
                std::pair<float, ConnectionState> result;
                switch (state) {
                case LogicState::low:
                    result.first = 0.0f;
                    result.second = ConnectionState::driven;
                    break;
                case LogicState::high:
                    result.first = 5.0f;
                    result.second = ConnectionState::driven;
                    break;
                case LogicState::high_z:
                    result.first = 0.0f;
                    result.second = ConnectionState::high_z;
                    break;
                case LogicState::unknown:
                    result.first =
                        (thresholds.highThreshold + thresholds.lowThreshold) /
                        2.0f;
                    result.second = ConnectionState::unknown;
                    break;
                }
                return result;
            }
        };

        struct SimulationEvent {
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

        struct SlotsGroupInfo {
            SlotsGroupType type = SlotsGroupType::none;
            bool isResizeable = false;
            size_t count = 0;
            std::vector<std::string> names;
            std::vector<std::pair<int, SlotCatergory>>
                categories; // slot_index, category
        };

        struct OperatorInfo {
            char op = '0';
            bool shouldNegateOutput = false;
        };

        struct ComponentState {
            std::vector<SlotState> inputStates;
            std::vector<bool> inputConnected;
            std::vector<SlotState> outputStates;
            std::vector<bool> outputConnected;
            bool isChanged = false;
            bool simError = false;
            std::any *auxData = nullptr;
            std::string errorMessage;
        };

        typedef std::function<ComponentState(const std::vector<SlotState> &,
                                             SimTime, const ComponentState &)>
            SimulationFunction;

    } // namespace SimEngine
}; // namespace Bess

REFLECT_ENUM(Bess::SimEngine::SimulationState)
REFLECT_ENUM(Bess::SimEngine::LogicState)
REFLECT_ENUM(Bess::SimEngine::SlotsGroupType)
REFLECT_ENUM(Bess::SimEngine::SlotCatergory)
REFLECT_ENUM(Bess::SimEngine::ComponentBehaviorType)
REFLECT_ENUM(Bess::SimEngine::SlotType)

REFLECT(Bess::SimEngine::SlotState, voltage, lastChangeTime)
REFLECT_VECTOR(Bess::SimEngine::SlotState)
REFLECT_VECTOR(bool)

REFLECT(Bess::SimEngine::ComponentState, inputStates, inputConnected,
        outputStates, outputConnected, isChanged, simError, errorMessage)

REFLECT_VECTOR(Bess::SimEngine::ComponentState)

REFLECT(Bess::SimEngine::ComponentPin, first, second)
REFLECT_VECTOR(Bess::SimEngine::ComponentPin)
REFLECT_VECTOR(std::vector<Bess::SimEngine::ComponentPin>)

REFLECT(SimEngine::OperatorInfo, op, shouldNegateOutput)

typedef std::pair<int, Bess::SimEngine::SlotCatergory> SlotCategoryPair;
REFLECT(SlotCategoryPair, first, second)
REFLECT_VECTOR(SlotCategoryPair)
REFLECT(Bess::SimEngine::SlotsGroupInfo, type, isResizeable, count, names,
        categories)
