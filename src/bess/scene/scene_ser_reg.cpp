#include "scene_ser_reg.h"
#include "common/logger.h"

namespace Bess::Canvas {
    std::unordered_map<std::string, SceneSerReg::DeSerFunc> SceneSerReg::m_registry;

    void SceneSerReg::registerComponent(const std::string &typeName, DeSerFunc func) {
        m_registry[typeName] = std::move(func);
    }

    std::shared_ptr<SceneComponent> SceneSerReg::createComponentFromJson(const Json::Value &j) {
        const auto &typeName = j["typeName"].asString();
        if (m_registry.contains(typeName)) {
            return m_registry[typeName](j);
        }
        BESS_WARN("[SceneSerReg] No registered deserialization function for component type {}",
                  typeName);
        return nullptr;
    }
} // namespace Bess::Canvas
