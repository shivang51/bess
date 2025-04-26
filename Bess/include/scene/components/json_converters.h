#pragma once
#include "glm.hpp"
#include "gtc/type_ptr.hpp"
#include "json.hpp"

namespace nlohmann {
    template <>
    struct adl_serializer<glm::vec4> {
        static void to_json(json &j, const glm::vec4 &vec) {
            j = json{vec.x, vec.y, vec.z, vec.w};
        }
        static void from_json(const json &j, glm::vec4 &vec) {
            j.at(0).get_to(vec.x);
            j.at(1).get_to(vec.y);
            j.at(2).get_to(vec.z);
            j.at(3).get_to(vec.w);
        }
    };
} // namespace nlohmann

namespace Bess::Canvas::Components {

    inline void to_json(nlohmann::json &j, const glm::vec2 &v) {
        j = nlohmann::json{v.x, v.y};
    }
    inline void from_json(const nlohmann::json &j, glm::vec2 &v) {
        j.at(0).get_to(v.x);
        j.at(1).get_to(v.y);
    }

    // glm::vec3
    inline void to_json(nlohmann::json &j, const glm::vec3 &v) {
        j = nlohmann::json{v.x, v.y, v.z};
    }
    inline void from_json(const nlohmann::json &j, glm::vec3 &v) {
        j.at(0).get_to(v.x);
        j.at(1).get_to(v.y);
        j.at(2).get_to(v.z);
    }

    // glm::vec4
    inline void to_json(nlohmann::json &j, const glm::vec4 &v) {
        j = nlohmann::json{v.x, v.y, v.z, v.w};
    }
    inline void from_json(const nlohmann::json &j, glm::vec4 &v) {
        j.at(0).get_to(v.x);
        j.at(1).get_to(v.y);
        j.at(2).get_to(v.z);
        j.at(3).get_to(v.w);
    }

    // glm::mat4: store as an array of 16 floats (row-major)
    inline void to_json(nlohmann::json &j, const glm::mat4 &m) {
        j = nlohmann::json{
            m[0][0], m[0][1], m[0][2], m[0][3],
            m[1][0], m[1][1], m[1][2], m[1][3],
            m[2][0], m[2][1], m[2][2], m[2][3],
            m[3][0], m[3][1], m[3][2], m[3][3]};
    }
    inline void from_json(const nlohmann::json &j, glm::mat4 &m) {
        float values[16];
        for (int i = 0; i < 16; ++i) {
            values[i] = j.at(i).get<float>();
        }
        m = glm::make_mat4(values);
    }

} // namespace Bess::Canvas::Components
