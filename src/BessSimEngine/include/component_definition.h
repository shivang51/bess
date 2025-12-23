#pragma once

#include "bess_api.h"
#include "types.h"
#include <string>
#include <vector>

namespace Bess::SimEngine {
#define MAKE_GETTER_SETTER_WC(type, name, varName, onChange) \
    const type &get##name() const { return varName; }        \
    void set##name(const type &value) {                      \
        varName = value;                                     \
        onChange();                                          \
    }                                                        \
    type &get##name() { return varName; }

#define MAKE_GETTER_SETTER(type, name, varName)       \
    const type &get##name() const { return varName; } \
    void set##name(const type &value) {               \
        varName = value;                              \
    }                                                 \
    type &get##name() { return varName; }

#define MAKE_GETTER(type, name, varName)              \
    const type &get##name() const { return varName; } \
    type &get##name() { return varName; }

    enum class SlotsGroupType : uint8_t {
        none,
        input,
        output
    };

    enum class SlotCatergory : uint8_t {
        none,
        clock,
        clear,
        enable,
    };

    enum class ComponentBehaviorType : uint8_t {
        none,
        input,
        output
    };

    struct BESS_API SlotsGroupInfo {
        SlotsGroupType type = SlotsGroupType::none;
        bool isResizeable = false;
        size_t count = 0;
        std::vector<std::string> names;
        std::vector<SlotCatergory> categories;
    };

    struct BESS_API OperatorInfo {
        char op = '0';
        bool shouldNegateOutput = false;
    };

    class BESS_API ComponentDefinition {
      public:
        ComponentDefinition() = default;

        ComponentDefinition(const ComponentDefinition &) = default;
        ComponentDefinition(ComponentDefinition &&) = default;
        ComponentDefinition &operator=(const ComponentDefinition &) = default;
        ComponentDefinition &operator=(ComponentDefinition &&) = default;

        virtual ~ComponentDefinition() = default;

        uint64_t getHash() const;

        MAKE_GETTER_SETTER(bool, ShouldAutoRechedule, m_shouldAutoReschedule)
        MAKE_GETTER_SETTER(SlotsGroupInfo, InputSlotsInfo, m_inputSlotsInfo)
        MAKE_GETTER_SETTER(SlotsGroupInfo, OutputSlotsInfo, m_outputSlotsInfo)
        MAKE_GETTER_SETTER(OperatorInfo, OpInfo, m_opInfo)
        MAKE_GETTER_SETTER(SimDelayNanoSeconds, SimDelay, m_simDelay)
        MAKE_GETTER_SETTER(std::string, Name, m_name)
        MAKE_GETTER_SETTER(std::string, GroupName, m_groupName)
        MAKE_GETTER_SETTER(ComponentBehaviorType, BehaviorType, m_behaviorType)
        MAKE_GETTER_SETTER(SimulationFunction, SimulationFunction, m_simulationFunction)
        MAKE_GETTER_SETTER(std::any, AuxData, m_auxData)

        template <typename T>
        T &getAuxDataAs() {
            return std::any_cast<T &>(m_auxData);
        }

        void computeHash();

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
        virtual bool onInputsResizeReq(SlotsGroupType groupType, size_t newSize);

        /**
         * This function returns the next simulation time, at which the comp should
         * be scheduled to be simulated.
         * returns: the offset or time after which it should be scheduled;
         * e.g.: if 10ns is returned the component will be scheduled to be simulated after
         * 10ns in the simulation engine
         **/
        virtual SimTime getNextSimTime();

        friend bool operator==(ComponentDefinition &a, ComponentDefinition &b) noexcept {
            return a.getHash() == b.getHash();
        }

      private:
        bool m_shouldAutoReschedule = false;
        SlotsGroupInfo m_inputSlotsInfo{}, m_outputSlotsInfo{};
        OperatorInfo m_opInfo{};
        SimDelayNanoSeconds m_simDelay = SimDelayNanoSeconds{0};
        ComponentBehaviorType m_behaviorType = ComponentBehaviorType::none;
        std::string m_name;
        std::string m_groupName;
        std::any m_auxData;
        uint64_t m_hash = 0;
        SimulationFunction m_simulationFunction = nullptr;
    };

} // namespace Bess::SimEngine
