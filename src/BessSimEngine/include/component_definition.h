#pragma once

#include "bess_api.h"
#include "type_map.h"
#include "types.h"
#include <memory>
#include <string>
#include <vector>

#include "../../Bess/common/log.h"

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

#define MAKE_SETTER(type, name, varName) \
    void set##name(const type &value) {  \
        varName = value;                 \
    }

    enum class CompDefIOGrowthPolicy : uint8_t {
        none,
        eq,
    };

    enum class CompDefinitionOwnership : uint8_t {
        NativeCpp,
        Python
    };

    struct BESS_API SlotsGroupInfo {
        SlotsGroupType type = SlotsGroupType::none;
        bool isResizeable = false;
        size_t count = 0;
        std::vector<std::string> names;
        std::vector<std::pair<int, SlotCatergory>> categories; // slot_index, category
    };

    struct BESS_API OperatorInfo {
        char op = '0';
        bool shouldNegateOutput = false;
    };

    class BESS_API Trait {
    };

    class BESS_API ComponentDefinition : public std::enable_shared_from_this<ComponentDefinition> {
      public:
        ComponentDefinition() = default;

        ComponentDefinition(const ComponentDefinition &) = default;
        ComponentDefinition(ComponentDefinition &&) = default;

        virtual ~ComponentDefinition() = default;

        uint64_t getHash() const;

        MAKE_GETTER_SETTER(bool, ShouldAutoReschedule, m_shouldAutoReschedule)
        MAKE_GETTER_SETTER(SlotsGroupInfo, InputSlotsInfo, m_inputSlotsInfo)
        MAKE_GETTER_SETTER(SlotsGroupInfo, OutputSlotsInfo, m_outputSlotsInfo)
        MAKE_GETTER_SETTER(OperatorInfo, OpInfo, m_opInfo)
        MAKE_SETTER(SimDelayNanoSeconds, SimDelay, m_simDelay)
        MAKE_GETTER_SETTER(std::string, Name, m_name)
        MAKE_GETTER_SETTER(std::string, GroupName, m_groupName)
        MAKE_GETTER_SETTER(ComponentBehaviorType, BehaviorType, m_behaviorType)
        MAKE_GETTER_SETTER(SimulationFunction, SimulationFunction, m_simulationFunction)
        MAKE_GETTER(std::any, AuxData, m_auxData)
        MAKE_GETTER_SETTER_WC(std::vector<std::string>,
                              OutputExpressions,
                              m_outputExpressions,
                              onExpressionsChange)
        MAKE_GETTER_SETTER(CompDefinitionOwnership, Ownership, m_ownership)
        MAKE_GETTER_SETTER(CompDefIOGrowthPolicy, IOGrowthPolicy, m_ioGrowthPolicy)

        template <typename T>
        T &getAuxDataAs() {
            return std::any_cast<T &>(m_auxData);
        }

        template <typename T>
        std::shared_ptr<T> getTrait() {
            auto itr = m_traits.find<T>();
            if (itr != m_traits.end()) {
                return std::static_pointer_cast<T>(itr->second);
            }
            return nullptr;
        }

        template <typename T>
        void addTrait(const std::shared_ptr<T> &trait) {
            m_traits.put<T>(trait);
        }

        template <typename T>
        void addTrait(T &&trait) {
            m_traits.put<T>(std::make_shared<T>(std::forward<T>(trait)));
        }

        template <typename T>
        void addTrait() {
            m_traits.put<T>(std::make_shared<T>(std::forward<T>(T())));
        }

        template <typename T>
        bool hasTrait() const {
            auto itr = m_traits.find<T>();
            return itr != m_traits.end();
        }

        void computeHash();

        /**
         * This function will compute the output expressions if needed.
         * i.e. when operator info is set and expressions are based on it.
         * returns: true if expressions were computed, false otherwise;
         **/
        virtual bool computeExpressionsIfNeeded();

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
        virtual bool onSlotsResizeReq(SlotsGroupType groupType, size_t newSize);

        /**
         * This function returns the next simulation time, at which the comp should
         * be scheduled to be simulated.
         * returns: the offset or time after which it should be scheduled;
         * e.g.: if 10ns is returned the component will be scheduled to be simulated after
         * 10ns in the simulation engine
         **/
        virtual SimDelayNanoSeconds getSimDelay() const;

        /**
         * This function returns the delay after which the component should be auto-rescheduled.
         * Will only be called if shouldAutoReschedule is true.
         * returns: the offset or time after which it should be rescheduled;
         * e.g.: if 10ns is returned the component will be rescheduled to be simulated after
         * 10ns in the simulation engine
         **/
        virtual SimTime getRescheduleDelay();

        /**
         * This function will be called when the component state changes;
         * oldState: previous state of the component;
         * newState: new state of the component;
         **/
        virtual void onStateChange(const ComponentState &oldState,
                                   const ComponentState &newState) {}

        virtual void onExpressionsChange();

        virtual std::shared_ptr<ComponentDefinition> clone() const;

        virtual void setAuxData(const std::any &data);

        friend bool operator==(ComponentDefinition &a, ComponentDefinition &b) noexcept {
            return a.getHash() == b.getHash();
        }

        friend bool operator==(const std::shared_ptr<ComponentDefinition> &a,
                               const std::shared_ptr<ComponentDefinition> &b) noexcept {
            return a->getHash() == b->getHash();
        }

      protected:
        bool m_shouldAutoReschedule = false;
        CompDefIOGrowthPolicy m_ioGrowthPolicy = CompDefIOGrowthPolicy::none;
        SlotsGroupInfo m_inputSlotsInfo{}, m_outputSlotsInfo{};
        OperatorInfo m_opInfo{};
        SimDelayNanoSeconds m_simDelay = SimDelayNanoSeconds{0};
        ComponentBehaviorType m_behaviorType = ComponentBehaviorType::none;
        std::string m_name;
        std::string m_groupName;
        std::any m_auxData;
        uint64_t m_hash = 0;
        SimulationFunction m_simulationFunction = nullptr;
        std::vector<std::string> m_outputExpressions; // A+B or A.B etc.
        TypeMap<std::shared_ptr<Trait>> m_traits;
        CompDefinitionOwnership m_ownership = CompDefinitionOwnership::NativeCpp;
    };

} // namespace Bess::SimEngine
