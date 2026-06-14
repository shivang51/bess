#include "bess_core/renderer/renderer_types.h"

namespace Bess::Core::Renderer {

    Json::Value Color::toJson() const {
        return toHex();
    }

    Color Color::fromJson(const Json::Value &json) {
        return Color::fromHex(json.asUInt());
    }

    float *Color::data() {
        return &r;
    }

} // namespace Bess::Core::Renderer

namespace Bess::JsonConvert {
    Json::Value toJson(const Core::Renderer::Color &color) {
        return color.toJson();
    }

    void fromJsonValue(const Json::Value &json, Core::Renderer::Color &color) {
        color = Core::Renderer::Color::fromJson(json);
    }
} // namespace Bess::JsonConvert
