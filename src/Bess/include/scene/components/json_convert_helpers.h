#pragma once

#include "bess_uuid.h"
#include "glm.hpp"
#include "json.hpp"

namespace nlohmann {
    inline void to_json(nlohmann::json &j, const Bess::UUID &uuid) {
        j = (uint64_t)uuid;
    }
    inline void from_json(const nlohmann::json &j, Bess::UUID &uuid) {
        uuid = j.get<uint64_t>();
    }

    inline void to_json(nlohmann::json &j, const glm::vec2 &vec) {
        j = nlohmann::json{vec.x, vec.y};
    }
    inline void from_json(const nlohmann::json &j, glm::vec2 &vec) {
        vec.x = j.at(0).get<float>();
        vec.y = j.at(1).get<float>();
    }

    // glm::vec3
    inline void to_json(nlohmann::json &j, const glm::vec3 &vec) {
        j = nlohmann::json{vec.x, vec.y, vec.z};
    }
    inline void from_json(const nlohmann::json &j, glm::vec3 &vec) {
        vec.x = j.at(0).get<float>();
        vec.y = j.at(1).get<float>();
        vec.z = j.at(2).get<float>();
    }

    // glm::vec4
    inline void to_json(nlohmann::json &j, const glm::vec4 &vec) {
        j = nlohmann::json{vec.x, vec.y, vec.z, vec.w};
    }
    inline void from_json(const nlohmann::json &j, glm::vec4 &vec) {
        vec.x = j.at(0).get<float>();
        vec.y = j.at(1).get<float>();
        vec.z = j.at(2).get<float>();
        vec.w = j.at(3).get<float>();
    }

    // glm::mat4 (stored as an array of 16 floats in column-major order)
    inline void to_json(nlohmann::json &j, const glm::mat4 &mat) {
        std::vector<float> vals;
        vals.reserve(16);
        // GLM matrices are column-major. We store row-major for simplicity.
        for (int row = 0; row < 4; ++row) {
            for (int col = 0; col < 4; ++col) {
                vals.push_back(mat[col][row]);
            }
        }
        j = vals;
    }
    inline void from_json(const nlohmann::json &j, glm::mat4 &mat) {
        std::vector<float> vals = j.get<std::vector<float>>();
        if (vals.size() != 16)
            throw std::runtime_error("Invalid matrix size");
        for (int row = 0; row < 4; ++row) {
            for (int col = 0; col < 4; ++col) {
                mat[col][row] = vals[row * 4 + col];
            }
        }
    }

} // namespace nlohmann