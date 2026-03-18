#pragma once

#include "bess_api.h"
#include "common/bess_uuid.h"
#include "component_definition.h"
#include "types.h"
#include <memory>

namespace Bess::SimEngine {

    typedef std::function<void(ComponentState &, ComponentState &)> TOnStateChangeCB;

    typedef std::function<void(size_t count)> TOnSlotCountChangeCB;

    struct BESS_API DigitalComponent {
        DigitalComponent() = default;
        DigitalComponent(const DigitalComponent &) = default;
        DigitalComponent(const std::shared_ptr<ComponentDefinition> &def,
                         bool cloneDef = true);

        size_t incrementInputCount(bool force = false);
        size_t incrementOutputCount(bool force = false);

        size_t decrementInputCount(bool force = false);
        size_t decrementOutputCount(bool force = false);

        void addOnStateChangeCB(const TOnStateChangeCB &cb);
        void addOnInputSlotCountChangeCB(const TOnSlotCountChangeCB &cb);
        void addOnOutputSlotCountChangeCB(const TOnSlotCountChangeCB &cb);

        void dispatchStateChange(ComponentState &oldState, ComponentState &newState);
        void dispatchInputSlotCountChange(size_t newCount);
        void dispatchOutputSlotCountChange(size_t newCount);

        UUID id;
        UUID netUuid = UUID::null;
        ComponentState state;
        std::shared_ptr<ComponentDefinition> definition;
        Connections inputConnections;
        Connections outputConnections;

      private:
        std::vector<TOnStateChangeCB> onStateChangeCbs;
        std::vector<TOnSlotCountChangeCB> onInputSlotCountChangeCbs;
        std::vector<TOnSlotCountChangeCB> onOutputSlotCountChangeCbs;
    };

} // namespace Bess::SimEngine

namespace Bess::JsonConvert {
    BESS_API void toJsonValue(Json::Value &j, const Bess::SimEngine::DigitalComponent &comp);
    BESS_API void fromJsonValue(const Json::Value &j, Bess::SimEngine::DigitalComponent &comp);
} // namespace Bess::JsonConvert
