#include "scene/scene_state/components/scene_component_types.h"
#include "bess_json/json_converters.h"

namespace Bess::JsonConvert {
    void toJsonValue(const Bess::Canvas::Transform &transform, Json::Value &j) {
        j["position"] = Json::Value(Json::arrayValue);
        j["position"].append(transform.position.x);
        j["position"].append(transform.position.y);
        j["position"].append(transform.position.z);

        j["scale"] = Json::Value(Json::arrayValue);
        j["scale"].append(transform.scale.x);
        j["scale"].append(transform.scale.y);

        j["angle"] = transform.angle;
    }

    void fromJsonValue(const Json::Value &j, Bess::Canvas::Transform &transform) {
        if (j.isMember("position") && j["position"].isArray() && j["position"].size() == 3) {
            transform.position.x = j["position"][0].asFloat();
            transform.position.y = j["position"][1].asFloat();
            transform.position.z = j["position"][2].asFloat();
        }

        if (j.isMember("scale") && j["scale"].isArray() && j["scale"].size() == 2) {
            transform.scale.x = j["scale"][0].asFloat();
            transform.scale.y = j["scale"][1].asFloat();
        }

        if (j.isMember("angle")) {
            transform.angle = j["angle"].asFloat();
        }
    }

    void toJsonValue(const Bess::Canvas::Style &style, Json::Value &j) {
        j["color"] = Json::Value(Json::arrayValue);
        j["color"].append(style.color.r);
        j["color"].append(style.color.g);
        j["color"].append(style.color.b);
        j["color"].append(style.color.a);

        j["borderColor"] = Json::Value(Json::arrayValue);
        j["borderColor"].append(style.borderColor.r);
        j["borderColor"].append(style.borderColor.g);
        j["borderColor"].append(style.borderColor.b);
        j["borderColor"].append(style.borderColor.a);

        j["borderSize"] = Json::Value(Json::arrayValue);
        j["borderSize"].append(style.borderSize.r);
        j["borderSize"].append(style.borderSize.g);
        j["borderSize"].append(style.borderSize.b);
        j["borderSize"].append(style.borderSize.a);

        j["borderRadius"] = Json::Value(Json::arrayValue);
        j["borderRadius"].append(style.borderRadius.r);
        j["borderRadius"].append(style.borderRadius.g);
        j["borderRadius"].append(style.borderRadius.b);
        j["borderRadius"].append(style.borderRadius.a);

        j["headerColor"] = Json::Value(Json::arrayValue);
        j["headerColor"].append(style.headerColor.r);
        j["headerColor"].append(style.headerColor.g);
        j["headerColor"].append(style.headerColor.b);
        j["headerColor"].append(style.headerColor.a);
    }

    void fromJsonValue(const Json::Value &j, Bess::Canvas::Style &style) {
        if (j.isMember("color") && j["color"].isArray() && j["color"].size() == 4) {
            style.color.r = j["color"][0].asFloat();
            style.color.g = j["color"][1].asFloat();
            style.color.b = j["color"][2].asFloat();
            style.color.a = j["color"][3].asFloat();
        }

        if (j.isMember("borderColor") && j["borderColor"].isArray() && j["borderColor"].size() == 4) {
            style.borderColor.r = j["borderColor"][0].asFloat();
            style.borderColor.g = j["borderColor"][1].asFloat();
            style.borderColor.b = j["borderColor"][2].asFloat();
            style.borderColor.a = j["borderColor"][3].asFloat();
        }

        if (j.isMember("borderSize") && j["borderSize"].isArray() && j["borderSize"].size() == 4) {
            style.borderSize.r = j["borderSize"][0].asFloat();
            style.borderSize.g = j["borderSize"][1].asFloat();
            style.borderSize.b = j["borderSize"][2].asFloat();
            style.borderSize.a = j["borderSize"][3].asFloat();
        }

        if (j.isMember("borderRadius") && j["borderRadius"].isArray() && j["borderRadius"].size() == 4) {
            style.borderRadius.r = j["borderRadius"][0].asFloat();
            style.borderRadius.g = j["borderRadius"][1].asFloat();
            style.borderRadius.b = j["borderRadius"][2].asFloat();
            style.borderRadius.a = j["borderRadius"][3].asFloat();
        }

        if (j.isMember("headerColor") && j["headerColor"].isArray() && j["headerColor"].size() == 4) {
            style.headerColor.r = j["headerColor"][0].asFloat();
            style.headerColor.g = j["headerColor"][1].asFloat();
            style.headerColor.b = j["headerColor"][2].asFloat();
            style.headerColor.a = j["headerColor"][3].asFloat();
        }
    }

    // Conn segment
    void toJsonValue(const Bess::Canvas::ConnSegment &segment, Json::Value &j) {
        toJsonValue(segment.offset, j["offset"]);

        j["orientation"] = static_cast<uint8_t>(segment.orientation);
    }

    void fromJsonValue(const Json::Value &j, Bess::Canvas::ConnSegment &segment) {
        if (j.isMember("offset")) {
            fromJsonValue(j["offset"], segment.offset);
        }

        if (j.isMember("orientation")) {
            segment.orientation = static_cast<Bess::Canvas::ConnSegOrientaion>(j["orientation"].asUInt());
        }
    }
} // namespace Bess::JsonConvert
