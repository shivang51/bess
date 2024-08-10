#include "common/helpers.h"

namespace Bess::Common {
    glm::vec3 Helpers::GetLeftCornerPos(const glm::vec3& pos, const glm::vec2& size) {
        return { pos.x - size.x / 2, pos.y + size.y / 2, pos.z };
    }

    float Helpers::JsonToFloat(const nlohmann::json json) {
        return static_cast<float>(json);
    }

    glm::vec3 Helpers::DecodeVec3(const nlohmann::json& json)
    {
        return glm::vec3({json["x"], json["y"], json["z"]});
    }

    nlohmann::json Helpers::EncodeVec3(const glm::vec3& val) {
        nlohmann::json data;
        data["x"] = val.x;
        data["y"] = val.y;
        data["z"] = val.z;
        return data;
    }

    Simulator::ComponentType Helpers::intToCompType(int type) {
        return static_cast<Simulator::ComponentType>(type);
    }

    UUIDv4::UUIDGenerator<std::mt19937_64> Helpers::uuidGenerator{};
}