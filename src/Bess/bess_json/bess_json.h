#include "bess_json/json_converters.h"
#include <json/json.h>
#include <type_traits>

#define GET_MACRO(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, NAME, ...) NAME

#define FOR_EACH(action, ...) \
    GET_MACRO(__VA_ARGS__, FE_20, FE_19, FE_18, FE_17, FE_16, FE_15, FE_14, FE_13, FE_12, FE_11, FE_10, FE_9, FE_8, FE_7, FE_6, FE_5, FE_4, FE_3, FE_2, FE_1)(action, __VA_ARGS__)

#define FE_1(m, x) m(x)
#define FE_2(m, x, ...) m(x) FE_1(m, __VA_ARGS__)
#define FE_3(m, x, ...) m(x) FE_2(m, __VA_ARGS__)
#define FE_4(m, x, ...) m(x) FE_3(m, __VA_ARGS__)
#define FE_5(m, x, ...) m(x) FE_4(m, __VA_ARGS__)
#define FE_6(m, x, ...) m(x) FE_5(m, __VA_ARGS__)
#define FE_7(m, x, ...) m(x) FE_6(m, __VA_ARGS__)
#define FE_8(m, x, ...) m(x) FE_7(m, __VA_ARGS__)
#define FE_9(m, x, ...) m(x) FE_8(m, __VA_ARGS__)
#define FE_10(m, x, ...) m(x) FE_9(m, __VA_ARGS__)
#define FE_11(m, x, ...) m(x) FE_10(m, __VA_ARGS__)
#define FE_12(m, x, ...) m(x) FE_11(m, __VA_ARGS__)
#define FE_13(m, x, ...) m(x) FE_12(m, __VA_ARGS__)
#define FE_14(m, x, ...) m(x) FE_13(m, __VA_ARGS__)
#define FE_15(m, x, ...) m(x) FE_14(m, __VA_ARGS__)
#define FE_16(m, x, ...) m(x) FE_15(m, __VA_ARGS__)
#define FE_17(m, x, ...) m(x) FE_16(m, __VA_ARGS__)
#define FE_18(m, x, ...) m(x) FE_17(m, __VA_ARGS__)
#define FE_19(m, x, ...) m(x) FE_18(m, __VA_ARGS__)
#define FE_20(m, x, ...) m(x) FE_19(m, __VA_ARGS__)

#define STRIP_PARENS(X) X
#define GET_KEY(k, g, s) #k
#define GET_GETTER(k, g, s) g
#define GET_SETTER(k, g, s) s

#define SERIALIZE_PROP(prop) \
    Bess::JsonConvert::toJsonValue(obj.GET_GETTER prop(), j[GET_KEY prop]);

// Deserialization Logic
#define DESERIALIZE_PROP(prop)                                   \
    if (j.isMember(GET_KEY prop)) {                              \
        /* Use decltype to find the return type of the getter */ \
        using T = std::decay_t<decltype(obj.GET_GETTER prop())>; \
        T temp;                                                  \
        Bess::JsonConvert::fromJsonValue(j[GET_KEY prop], temp); \
        obj.GET_SETTER prop(temp);                               \
    }

#define REFLECT_PROPS(className, ...)                                     \
    namespace Bess::JsonConvert {                                         \
        inline void toJsonValue(const className &obj, Json::Value &j) {   \
            j = Json::objectValue;                                        \
            FOR_EACH(SERIALIZE_PROP, __VA_ARGS__)                         \
        }                                                                 \
        inline void fromJsonValue(const Json::Value &j, className &obj) { \
            if (j.isObject()) {                                           \
                FOR_EACH(DESERIALIZE_PROP, __VA_ARGS__)                   \
            }                                                             \
        }                                                                 \
    }

// Helper logic for Enum Strings
#define FOR_EACH_ENUM(action, type, ...) \
    GET_MACRO(__VA_ARGS__, FE_E_20, FE_E_19, FE_E_18, FE_E_17, FE_E_16, FE_E_15, FE_E_14, FE_E_13, FE_E_12, FE_E_11, FE_E_10, FE_E_9, FE_E_8, FE_E_7, FE_E_6, FE_E_5, FE_E_4, FE_E_3, FE_E_2, FE_E_1)(action, type, __VA_ARGS__)

#define FE_E_1(m, t, x) m(t, x)
#define FE_E_2(m, t, x, ...) m(t, x) FE_E_1(m, t, __VA_ARGS__)
#define FE_E_3(m, t, x, ...) m(t, x) FE_E_2(m, t, __VA_ARGS__)
#define FE_E_4(m, t, x, ...) m(t, x) FE_E_3(m, t, __VA_ARGS__)
#define FE_E_5(m, t, x, ...) m(t, x) FE_E_4(m, t, __VA_ARGS__)
#define FE_E_6(m, t, x, ...) m(t, x) FE_E_5(m, t, __VA_ARGS__)
#define FE_E_7(m, t, x, ...) m(t, x) FE_E_6(m, t, __VA_ARGS__)
#define FE_E_8(m, t, x, ...) m(t, x) FE_E_7(m, t, __VA_ARGS__)
#define FE_E_9(m, t, x, ...) m(t, x) FE_E_8(m, t, __VA_ARGS__)
#define FE_E_10(m, t, x, ...) m(t, x) FE_E_9(m, t, __VA_ARGS__)
#define FE_E_11(m, t, x, ...) m(t, x) FE_E_10(m, t, __VA_ARGS__)
#define FE_E_12(m, t, x, ...) m(t, x) FE_E_11(m, t, __VA_ARGS__)
#define FE_E_13(m, t, x, ...) m(t, x) FE_E_12(m, t, __VA_ARGS__)
#define FE_E_14(m, t, x, ...) m(t, x) FE_E_13(m, t, __VA_ARGS__)
#define FE_E_15(m, t, x, ...) m(t, x) FE_E_14(m, t, __VA_ARGS__)
#define FE_E_16(m, t, x, ...) m(t, x) FE_E_15(m, t, __VA_ARGS__)
#define FE_E_17(m, t, x, ...) m(t, x) FE_E_16(m, t, __VA_ARGS__)
#define FE_E_18(m, t, x, ...) m(t, x) FE_E_17(m, t, __VA_ARGS__)
#define FE_E_19(m, t, x, ...) m(t, x) FE_E_18(m, t, __VA_ARGS__)
#define FE_E_20(m, t, x, ...) m(t, x) FE_E_19(m, t, __VA_ARGS__)

#define ENUM_TO_STR_CASE(type, val) \
    if (v == type::val) {           \
        j = #val;                   \
        return;                     \
    }
#define STR_TO_ENUM_CASE(type, val) \
    if (j.asString() == #val) {     \
        v = type::val;              \
        return;                     \
    }

#define REFLECT_ENUM(EnumType, ...)                                    \
    namespace Bess::JsonConvert {                                      \
        inline void toJsonValue(const EnumType &v, Json::Value &j) {   \
            FOR_EACH_ENUM(ENUM_TO_STR_CASE, EnumType, __VA_ARGS__)     \
            j = (int)v; /* Fallback to int if not found */             \
        }                                                              \
        inline void fromJsonValue(const Json::Value &j, EnumType &v) { \
            if (j.isString()) {                                        \
                FOR_EACH_ENUM(STR_TO_ENUM_CASE, EnumType, __VA_ARGS__) \
            } else if (j.isInt()) {                                    \
                v = static_cast<EnumType>(j.asInt());                  \
            }                                                          \
        }                                                              \
    }
