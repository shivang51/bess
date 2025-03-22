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
