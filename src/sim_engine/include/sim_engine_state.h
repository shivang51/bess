#pragma once

#include "common/bess_uuid.h"
#include "drivers/sim_driver.h"
#include "net/net.h"
#include <memory>
#include <unordered_map>

namespace Bess::SimEngine {
    class SimEngineState {
      public:
        SimEngineState();
        ~SimEngineState();

        void reset();

        typedef Drivers::SimComponent TSimComp;

        std::vector<std::shared_ptr<TSimComp>> findCompsByName(
            const std::string &name) const;

        bool isComponentValid(const UUID &uuid) const;

        void addComponent(const std::shared_ptr<TSimComp> &comp);
        void removeComponent(const UUID &uuid);

        const std::unordered_map<UUID, std::shared_ptr<TSimComp>> &getComponents() const;

        void clearComponents();

        template <typename T>
        std::shared_ptr<T> getComponent(const UUID &uuid) const {
            auto comp = getComponent(uuid);
            if (comp) {
                return std::dynamic_pointer_cast<T>(comp);
            }
            return nullptr;
        }

        std::shared_ptr<TSimComp> getComponent(const UUID &uuid) const;

        void addNet(const Net &net);
        void removeNet(const UUID &uuid);

        const std::unordered_map<UUID, Net> &getNetsMap() const;

        void clearNets();

      private:
        std::unordered_map<UUID, std::shared_ptr<TSimComp>> m_components;
        std::unordered_map<UUID, Net> m_nets;
    };
} // namespace Bess::SimEngine

namespace Bess::JsonConvert {
    void toJsonValue(const Bess::SimEngine::SimEngineState &state, Json::Value &j);
    void fromJsonValue(const Json::Value &j, Bess::SimEngine::SimEngineState &state);
} // namespace Bess::JsonConvert
