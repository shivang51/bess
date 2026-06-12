#include "bess_core/renderer/renderer_types.h"

namespace Bess::Core::Renderer {

    Json::Value Color::toJson() const {
        Json::Value json;
        json["r"] = r;
        json["g"] = g;
        json["b"] = b;
        json["a"] = a;
        return json;
    }

    Color Color::fromJson(const Json::Value &json) {
        Color color;
        if (json.isMember("r") && json["r"].isNumeric())
            color.r = json["r"].asFloat();
        if (json.isMember("g") && json["g"].isNumeric())
            color.g = json["g"].asFloat();
        if (json.isMember("b") && json["b"].isNumeric())
            color.b = json["b"].asFloat();
        if (json.isMember("a") && json["a"].isNumeric())
            color.a = json["a"].asFloat();
        return color;
    }
} // namespace Bess::Core::Renderer
