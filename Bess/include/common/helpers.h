#pragma once
#include "uuid_v4.h"
#include "glm.hpp"
#include "components_manager/component_type.h"

#include "json.hpp"

namespace Bess::Common {
    class Helpers {
    public:
        static glm::vec3 GetLeftCornerPos(const glm::vec3& pos, const glm::vec2& size);

        static float JsonToFloat(const nlohmann::json json);
        
        static glm::vec3 DecodeVec3(const nlohmann::json& json);

        static nlohmann::json EncodeVec3(const glm::vec3& val);
        
        static Simulator::ComponentType intToCompType(int type);

        static UUIDv4::UUIDGenerator<std::mt19937_64> uuidGenerator;
    };
}