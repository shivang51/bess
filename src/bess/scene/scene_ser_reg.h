#pragma once
#include "bess_json/bess_json.h"
#include <functional>
#include <memory>
#include <string>

namespace Bess::Canvas {
    class SceneComponent;

    class SceneSerReg {
      public:
        typedef std::function<std::shared_ptr<SceneComponent>(const Json::Value &)> DeSerFunc;

        static void registerComponent(const std::string &typeName, DeSerFunc func);

        static std::shared_ptr<SceneComponent> createComponentFromJson(const Json::Value &j);

      private:
        static std::unordered_map<std::string, DeSerFunc> m_registry;
    };
} // namespace Bess::Canvas
