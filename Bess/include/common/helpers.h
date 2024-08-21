#pragma once
#include "glm.hpp"
#include "components_manager/component_type.h"

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

        static glm::vec3 GetLeftCornerPos(const glm::vec3& pos, const glm::vec2& size);

        static float JsonToFloat(const nlohmann::json json);
        
        static glm::vec3 DecodeVec3(const nlohmann::json& json);

        static nlohmann::json EncodeVec3(const glm::vec3& val);
        
        static Simulator::ComponentType intToCompType(int type);

        static UUIDGenerator uuidGenerator;

        static std::string uuidToStr(const uuids::uuid& uid);
        
        static uuids::uuid strToUUID(const std::string& uuid);
        
        static float calculateTextWidth(const std::string& text, float fontSize);

        static float getAnyCharHeight(float fontSize, char ch = '(');

        static std::string toLowerCase(const std::string& str);
    };
}