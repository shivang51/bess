#pragma once

#include "common/bess_api.h"
#include "bess_json/bess_json.h"
#include <functional>
#include <memory>
#include <string>

namespace Bess::Canvas {
    class SceneComponent;

    class BESS_API SceneSerReg {
      public:
        typedef std::function<std::shared_ptr<SceneComponent>(
            const Json::Value &)>
            DeSerFunc;

        static bool hasComponent(const std::string &typeName);

        static void registerComponent(const std::string &typeName,
                                      DeSerFunc func);

        static std::shared_ptr<SceneComponent>
        createComponentFromJson(const Json::Value &j);

        static void setFallback(DeSerFunc func);
        static void clearRegistry();

      private:
        static std::unordered_map<std::string, SceneSerReg::DeSerFunc> &
        getRegistry();
        static DeSerFunc &getFallback();
    };
} // namespace Bess::Canvas
