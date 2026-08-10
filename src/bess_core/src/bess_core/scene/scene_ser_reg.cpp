#include "bess_core/scene/scene_ser_reg.h"
#include "common/logger.h"

namespace Bess::Canvas {

    void SceneSerReg::registerComponent(const std::string &typeName,
                                        DeSerFunc func) {
        auto &m_registry = getRegistry();
        m_registry[typeName] = std::move(func);
    }

    std::shared_ptr<SceneComponent>
    SceneSerReg::createComponentFromJson(const Json::Value &j) {
        const auto &m_registry = getRegistry();
        const auto &typeName = j["typeName"].asString();
        if (m_registry.contains(typeName)) {
            return m_registry.at(typeName)(j);
        }
        if (const auto &fallback = getFallback()) {
            return fallback(j);
        }
        BESS_WARN("[SceneSerReg] No registered deserialization function for "
                  "component type {}",
                  typeName);
        return nullptr;
    }

    void SceneSerReg::clearRegistry() {
        auto &m_registry = getRegistry();
        m_registry.clear();
        getFallback() = {};
    }

    void SceneSerReg::setFallback(DeSerFunc func) {
        getFallback() = std::move(func);
    }

    std::unordered_map<std::string, SceneSerReg::DeSerFunc> &
    SceneSerReg::getRegistry() {
        static std::unordered_map<std::string, DeSerFunc> registry;
        return registry;
    }

    SceneSerReg::DeSerFunc &SceneSerReg::getFallback() {
        static DeSerFunc fallback;
        return fallback;
    }

    bool SceneSerReg::hasComponent(const std::string &typeName) {
        const auto &m_registry = getRegistry();
        return m_registry.contains(typeName);
    }
} // namespace Bess::Canvas
