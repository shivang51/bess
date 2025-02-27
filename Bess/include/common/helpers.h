#pragma once
#include "component_catalog.h"
#include "glm.hpp"

#include "json.hpp"
#include <uuid.h>

namespace Bess::Common {
    class Helpers {
      public:
        class UUIDGenerator {
          public:
            UUIDGenerator();

            uuids::uuid getUUID();

          private:
            uuids::uuid_random_generator m_gen;
        };

        static glm::vec3 GetLeftCornerPos(const glm::vec3 &pos, const glm::vec2 &size);

        static float JsonToFloat(const nlohmann::json json);

        static glm::vec3 DecodeVec3(const nlohmann::json &json);

        static glm::vec4 DecodeVec4(const nlohmann::json &json);

        static nlohmann::json EncodeVec3(const glm::vec3 &val);

        static nlohmann::json EncodeVec4(const glm::vec4 &val);

        static SimEngine::ComponentType intToCompType(int type);

        static UUIDGenerator uuidGenerator;

        static std::string uuidToStr(const uuids::uuid &uid);

        static uuids::uuid strToUUID(const std::string &uuid);

        static float calculateTextWidth(const std::string &text, float fontSize);

        static float getAnyCharHeight(float fontSize, char ch = '(');

        static std::string toLowerCase(const std::string &str);
    };
} // namespace Bess::Common
