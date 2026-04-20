#pragma once

#include "bess_api.h"
#include "common/bess_uuid.h"
#include "component_definition.h"
#include "types.h"
#include <memory>

namespace Bess::SimEngine {

    // oldState, newState
    typedef std::function<void(const ComponentState &, const ComponentState &)> TOnStateChangeCB;

    typedef std::function<void(size_t count)> TOnSlotCountChangeCB;

    class BESS_API DigitalComponent {
      public:
        DigitalComponent() = default;
        DigitalComponent(const DigitalComponent &) = default;
        DigitalComponent(const std::shared_ptr<ComponentDefinition> &def,
                         bool cloneDef = true);

        size_t incrementInputCount(bool force = false);
        size_t incrementOutputCount(bool force = false);

        size_t decrementInputCount(bool force = false);
        size_t decrementOutputCount(bool force = false);

        void addOnStateChangeCB(const UUID &id, const TOnStateChangeCB &cb);
        void addOnInputSlotCountChangeCB(const UUID &id, const TOnSlotCountChangeCB &cb);
        void addOnOutputSlotCountChangeCB(const UUID &id, const TOnSlotCountChangeCB &cb);

        void removeOnStateChangeCB(const UUID &id);
        void removeOnInputSlotCountChangeCB(const UUID &id);
        void removeOnOutputSlotCountChangeCB(const UUID &id);

        void dispatchStateChange(ComponentState &oldState, ComponentState &newState);
        void dispatchInputSlotCountChange(size_t newCount);
        void dispatchOutputSlotCountChange(size_t newCount);

        void clearCallbacks();

        MAKE_GETTER_SETTER(std::string, Name, m_name)

        UUID id;
        UUID netUuid = UUID::null;
        ComponentState state;
        std::shared_ptr<ComponentDefinition> definition;
        Connections inputConnections;
        Connections outputConnections;

      private:
        static std::unordered_map<std::string, int> &getNameCountMap();

      private:
        std::string m_name;
        std::vector<std::pair<UUID, TOnStateChangeCB>> m_onStateChangeCbs;
        std::vector<std::pair<UUID, TOnSlotCountChangeCB>> m_onInputSlotCountChangeCbs;
        std::vector<std::pair<UUID, TOnSlotCountChangeCB>> m_onOutputSlotCountChangeCbs;
    };

} // namespace Bess::SimEngine

namespace Bess::JsonConvert {
    BESS_API void toJsonValue(Json::Value &j, const Bess::SimEngine::DigitalComponent &comp);
    BESS_API void fromJsonValue(const Json::Value &j, Bess::SimEngine::DigitalComponent &comp);
} // namespace Bess::JsonConvert
