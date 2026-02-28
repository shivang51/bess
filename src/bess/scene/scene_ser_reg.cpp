#include "scene_ser_reg.h"
#include "common/logger.h"

namespace Bess::Canvas {

    void SceneSerReg::registerComponent(const std::string &typeName, DeSerFunc func) {
        auto &m_registry = getRegistry();
        m_registry[typeName] = std::move(func);
    }

    std::shared_ptr<SceneComponent> SceneSerReg::createComponentFromJson(const Json::Value &j) {
        const auto &m_registry = getRegistry();
        const auto &typeName = j["typeName"].asString();
        if (m_registry.contains(typeName)) {
            return m_registry.at(typeName)(j);
        }
        BESS_WARN("[SceneSerReg] No registered deserialization function for component type {}",
                  typeName);
        return nullptr;
    }

    void SceneSerReg::clearRegistry() {
        auto &m_registry = getRegistry();
        m_registry.clear();
    }

    std::unordered_map<std::string, SceneSerReg::DeSerFunc> &SceneSerReg::getRegistry() {
        static std::unordered_map<std::string, DeSerFunc> registry;
        return registry;
    }
} // namespace Bess::Canvas
