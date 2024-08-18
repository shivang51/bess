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

    std::string Helpers::uuidToStr(const uuids::uuid& uid)
    {
        return uuids::to_string(uid);
    }

    uuids::uuid Helpers::strToUUID(const std::string& uuid) {
        return uuids::uuid::from_string(uuid).value();
    }

    Helpers::UUIDGenerator Helpers::uuidGenerator = Helpers::UUIDGenerator();

    Helpers::UUIDGenerator::UUIDGenerator()
    {
        std::random_device rd;
        auto seed_data = std::array<int, std::mt19937::state_size> {};
        std::generate(std::begin(seed_data), std::end(seed_data), std::ref(rd));
        std::seed_seq seq(std::begin(seed_data), std::end(seed_data));
        std::mt19937 generator(seq);
        m_gen = uuids::uuid_random_generator{generator};
    }

    uuids::uuid Helpers::UUIDGenerator::getUUID()
    {
        std::random_device rd;
        auto seed_data = std::array<int, std::mt19937::state_size> {};
        std::generate(std::begin(seed_data), std::end(seed_data), std::ref(rd));
        std::seed_seq seq(std::begin(seed_data), std::end(seed_data));
        std::mt19937 generator(seq);
        uuids::uuid_random_generator gen{ generator };
        return gen();
    }
}