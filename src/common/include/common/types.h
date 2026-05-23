#pragma once

#include "common/bess_uuid.h"
#include <any>
#include <chrono>
#include <ratio>

namespace Bess {
    using TimeMs = std::chrono::duration<double, std::milli>;
    using TimeNs = std::chrono::duration<double, std::nano>;

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

        struct SlotState {
            LogicState state = LogicState::low;
            SimTime lastChangeTime{0};

            SlotState() = default;

            SlotState(LogicState state, SimTime time) {
                this->state = state;
                lastChangeTime = time;
            }

            SlotState(bool value) {
                state = value ? LogicState::high : LogicState::low;
            }

            explicit operator bool() const { return state == LogicState::high; }

            SlotState &operator=(const bool &val) {
                this->state = val ? LogicState::high : LogicState::low;
                return *this;
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

REFLECT(Bess::SimEngine::SlotState, state, lastChangeTime)
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
