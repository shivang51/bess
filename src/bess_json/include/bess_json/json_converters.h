#pragma once
#include "glm.hpp"
#include "gtc/type_ptr.hpp"
#include "json/json.h"
#include <chrono>
#include <string>
#include <unordered_set>

namespace Bess::JsonConvert {
    // Primitives
    inline void toJsonValue(const char &v, Json::Value &j) { j = static_cast<int>(v); }
    inline void toJsonValue(const int &v, Json::Value &j) { j = v; }
    inline void toJsonValue(const float &v, Json::Value &j) { j = v; }
    inline void toJsonValue(const double &v, Json::Value &j) { j = v; }
    inline void toJsonValue(const std::string &v, Json::Value &j) { j = v; }
    inline void toJsonValue(const bool &v, Json::Value &j) { j = v; }
    inline void toJsonValue(const uint32_t &v, Json::Value &j) { j = Json::UInt(v); }
    inline void toJsonValue(const uint64_t &v, Json::Value &j) { j = Json::UInt64(v); }

    // std::chrono::duration
    template <typename Rep, typename Period>
    void toJsonValue(const std::chrono::duration<Rep, Period> &duration, Json::Value &j) {
        j = static_cast<double>(std::chrono::duration_cast<std::chrono::duration<double>>(duration).count());
    }

    // Vectors
    template <typename T>
    void toJsonValue(const std::vector<T> &vec, Json::Value &j) {
        j = Json::arrayValue;
        for (const auto &item : vec) {
            Json::Value val;
            toJsonValue(item, val);
            j.append(val);
        }
    }

    // Vectors
    template <typename T>
    void toJsonValue(const std::unordered_set<T> &vec, Json::Value &j) {
        j = Json::arrayValue;
        for (const auto &item : vec) {
            Json::Value val;
            toJsonValue(item, val);
            j.append(val);
        }
    }

    // --- FROM JSON ---
    inline void fromJsonValue(const Json::Value &j, char &v) {
        if (j.isInt())
            v = static_cast<char>(j.asInt());
    }

    inline void fromJsonValue(const Json::Value &j, int &v) {
        if (j.isInt())
            v = j.asInt();
    }

    inline void fromJsonValue(const Json::Value &j, float &v) {
        if (j.isNumeric())
            v = j.asFloat();
    }

    inline void fromJsonValue(const Json::Value &j, double &v) {
        if (j.isNumeric())
            v = j.asDouble();
    }

    inline void fromJsonValue(const Json::Value &j, std::string &v) {
        if (j.isString())
            v = j.asString();
    }

    inline void fromJsonValue(const Json::Value &j, bool &v) {
        if (j.isBool())
            v = j.asBool();
    }

    inline void fromJsonValue(const Json::Value &j, uint32_t &v) {
        if (j.isUInt())
            v = j.asUInt();
    }

    inline void fromJsonValue(const Json::Value &j, uint64_t &v) {
        if (j.isUInt64())
            v = j.asUInt64();
    }

    template <typename Rep, typename Period>
    void fromJsonValue(const Json::Value &j, std::chrono::duration<Rep, Period> &duration) {
        if (j.isNumeric()) {
            double count = j.asDouble();
            duration = std::chrono::duration<Rep, Period>(static_cast<Rep>(count));
        }
    }

    template <typename T>
    void fromJsonValue(const Json::Value &j, std::vector<T> &vec) {
        vec.clear();
        if (j.isArray()) {
            for (const auto &item : j) {
                T val;
                fromJsonValue(item, val);
                vec.push_back(val);
            }
        }
    }

    template <typename T>
    void fromJsonValue(const Json::Value &j, std::unordered_set<T> &vec) {
        vec.clear();
        if (j.isArray()) {
            for (const auto &item : j) {
                T val;
                fromJsonValue(item, val);
                vec.insert(val);
            }
        }
    }

    // --- glm::vec2 ---
    inline void toJsonValue(const glm::vec2 &vec, Json::Value &j) {
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
                Json::ArrayIndex index = (row * 4) + col;
                mat[col][row] = j[index].asFloat();
            }
        }
    }
} // namespace Bess::JsonConvert
