#pragma once

#include "common/bess_uuid.h"
#include "digital_component.h"
#include "net/net.h"
#include <memory>
#include <unordered_map>

namespace Bess::SimEngine {
    class SimEngineState {
      public:
        SimEngineState();
        ~SimEngineState();

        void reset();

        bool isComponentValid(const UUID &uuid) const;

        void addDigitalComponent(const std::shared_ptr<DigitalComponent> &comp);
        void removeDigitalComponent(const UUID &uuid);

        const std::unordered_map<UUID, std::shared_ptr<DigitalComponent>> &getDigitalComponents() const;

        void clearDigitalComponents();

        std::shared_ptr<DigitalComponent> getDigitalComponent(const UUID &uuid) const;

        void addNet(const Net &net);
        void removeNet(const UUID &uuid);

        const std::unordered_map<UUID, Net> &getNetsMap() const;

        void clearNets();

      private:
        std::unordered_map<UUID, std::shared_ptr<DigitalComponent>> m_digitalComponents;
        std::unordered_map<UUID, Net> m_nets;
    };
} // namespace Bess::SimEngine

namespace Bess::JsonConvert {
    void toJsonValue(const Bess::SimEngine::SimEngineState &state, Json::Value &j);
    void fromJsonValue(const Json::Value &j, Bess::SimEngine::SimEngineState &state);
} // namespace Bess::JsonConvert
