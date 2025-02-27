#include "common/helpers.h"
#include "scene/renderer/renderer.h"
namespace Bess::Common {
    glm::vec3 Helpers::GetLeftCornerPos(const glm::vec3 &pos, const glm::vec2 &size) {
        return {pos.x - size.x / 2, pos.y - size.y / 2, pos.z};
    }

    float Helpers::JsonToFloat(const nlohmann::json json) {
        return static_cast<float>(json);
    }

    glm::vec3 Helpers::DecodeVec3(const nlohmann::json &json) {
        return glm::vec3({json["x"], json["y"], json["z"]});
    }

    glm::vec4 Helpers::DecodeVec4(const nlohmann::json &json) {
        return glm::vec4({json["x"], json["y"], json["z"], json["w"]});
    }

    nlohmann::json Helpers::EncodeVec3(const glm::vec3 &val) {
        nlohmann::json data;
        data["x"] = val.x;
        data["y"] = val.y;
        data["z"] = val.z;
        return data;
    }

    nlohmann::json Helpers::EncodeVec4(const glm::vec4 &val) {
        nlohmann::json data;
        data["x"] = val.x;
        data["y"] = val.y;
        data["z"] = val.z;
        data["w"] = val.w;
        return data;
    }

    SimEngine::ComponentType Helpers::intToCompType(int type) {
        return static_cast<SimEngine::ComponentType>(type);
    }

    std::string Helpers::uuidToStr(const uuids::uuid &uid) {
        return uuids::to_string(uid);
    }

    uuids::uuid Helpers::strToUUID(const std::string &uuid) {
        return uuids::uuid::from_string(uuid).value();
    }

    Helpers::UUIDGenerator Helpers::uuidGenerator = Helpers::UUIDGenerator();

    Helpers::UUIDGenerator::UUIDGenerator() {
        std::random_device rd;
        auto seed_data = std::array<int, std::mt19937::state_size>{};
        std::generate(std::begin(seed_data), std::end(seed_data), std::ref(rd));
        std::seed_seq seq(std::begin(seed_data), std::end(seed_data));
        std::mt19937 generator(seq);
        m_gen = uuids::uuid_random_generator{generator};
    }

    uuids::uuid Helpers::UUIDGenerator::getUUID() {
        std::random_device rd;
        auto seed_data = std::array<int, std::mt19937::state_size>{};
        std::generate(std::begin(seed_data), std::end(seed_data), std::ref(rd));
        std::seed_seq seq(std::begin(seed_data), std::end(seed_data));
        std::mt19937 generator(seq);
        uuids::uuid_random_generator gen{generator};
        return gen();
    }

    float Helpers::calculateTextWidth(const std::string &text, float fontSize) {
        float w = 0.f;
        for (auto &ch_ : text) {
            auto ch = Renderer2D::Renderer::getCharRenderSize(ch_, fontSize);
            w += ch.x;
        }
        return w;
    }

    float Helpers::getAnyCharHeight(float fontSize, char ch) {
        return Renderer2D::Renderer::getCharRenderSize(ch, fontSize).y;
    }

    std::string Helpers::toLowerCase(const std::string &str) {
        std::string data = str;
        std::transform(data.begin(), data.end(), data.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        return data;
    }
} // namespace Bess::Common
