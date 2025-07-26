#pragma once
#include "glm.hpp"
#include "gtc/type_ptr.hpp"
#include "json/json.h"
#include "comp_json_converters.h"

namespace Bess::JsonConvert {
    // --- glm::vec2 ---
    inline void toJsonValue(const glm::vec2 &vec, Json::Value &j) {
        // Explicitly create an array and append elements.
        j = Json::Value(Json::arrayValue);
        j.append(vec.x);
        j.append(vec.y);
    }

    /**
     * @brief Converts a Json::Value array back to a glm::vec2.
     * @param j The source Json::Value.
     * @param vec The destination vector to be populated.
     */
    inline void fromJsonValue(const Json::Value &j, glm::vec2 &vec) {
        if (!j.isArray() || j.size() != 2) {
            throw std::runtime_error("Bess::JsonHelpers: Invalid JSON for glm::vec2. Must be an array of size 2.");
        }
        // Access elements by index and use the 'as' methods for type conversion.
        vec.x = j[0].asFloat();
        vec.y = j[1].asFloat();
    }

    // --- glm::vec3 ---

    inline void toJsonValue(const glm::vec3 &vec, Json::Value &j) {
        j = Json::Value(Json::arrayValue);
        j.append(vec.x);
        j.append(vec.y);
        j.append(vec.z);
    }

    inline void fromJsonValue(const Json::Value &j, glm::vec3 &vec) {
        if (!j.isArray() || j.size() != 3) {
            throw std::runtime_error("Bess::JsonHelpers: Invalid JSON for glm::vec3. Must be an array of size 3.");
        }
        vec.x = j[0].asFloat();
        vec.y = j[1].asFloat();
        vec.z = j[2].asFloat();
    }

    // --- glm::vec4 ---

    inline void toJsonValue(const glm::vec4 &vec, Json::Value &j) {
        j = Json::Value(Json::arrayValue);
        j.append(vec.x);
        j.append(vec.y);
        j.append(vec.z);
        j.append(vec.w);
    }

    inline void fromJsonValue(const Json::Value &j, glm::vec4 &vec) {
        if (!j.isArray() || j.size() != 4) {
            throw std::runtime_error("Bess::JsonHelpers: Invalid JSON for glm::vec4. Must be an array of size 4.");
        }
        vec.x = j[0].asFloat();
        vec.y = j[1].asFloat();
        vec.z = j[2].asFloat();
        vec.w = j[3].asFloat();
    }

    // --- glm::mat4 ---

    /**
     * @brief Converts a glm::mat4 to a Json::Value array of 16 floats.
     * @param mat The source matrix.
     * @param j The destination Json::Value to be populated.
     */
    inline void toJsonValue(const glm::mat4 &mat, Json::Value &j) {
        j = Json::Value(Json::arrayValue);
        // GLM matrices are column-major. We store row-major for simplicity and readability.
        for (int row = 0; row < 4; ++row) {
            for (int col = 0; col < 4; ++col) {
                j.append(mat[col][row]);
            }
        }
    }

    /**
     * @brief Converts a Json::Value array of 16 floats back to a glm::mat4.
     * @param j The source Json::Value.
     * @param mat The destination matrix to be populated.
     */
    inline void fromJsonValue(const Json::Value &j, glm::mat4 &mat) {
        if (!j.isArray() || j.size() != 16) {
            throw std::runtime_error("Bess::JsonHelpers: Invalid JSON for glm::mat4. Must be an array of size 16.");
        }
        for (int row = 0; row < 4; ++row) {
            for (int col = 0; col < 4; ++col) {
                // The index into the flat array
                Json::ArrayIndex index = row * 4 + col;
                mat[col][row] = j[index].asFloat();
            }
        }
    }
} // namespace Bess::Canvas::Components
