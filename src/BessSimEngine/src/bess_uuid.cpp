#include "bess_uuid.h"
#include <random>

namespace Bess {
    // thanks to Mr. Cherno
    static std::random_device s_RandomDevice;
    static std::mt19937_64 s_Engine(s_RandomDevice());
    static std::uniform_int_distribution<uint64_t> s_UniformDistribution;

    constexpr UUID UUID::null = UUID(0);
    constexpr UUID UUID::master = UUID(9);

    UUID::UUID() : m_UUID(s_UniformDistribution(s_Engine)) {
    }

    std::string UUID::toString() const noexcept {
        return std::to_string(m_UUID);
    }

    namespace JsonConvert {
        BESS_API void toJsonValue(const Bess::UUID &uuid, Json::Value &j) {
            j = (Json::UInt64)uuid;
        }

        BESS_API void fromJsonValue(const Json::Value &j, Bess::UUID &uuid) {
            uuid = j.asUInt64();
        }
    } // namespace JsonConvert

    UUID UUID::fromString(const std::string &str) noexcept {
        return {static_cast<uint64_t>(std::stoull(str))};
    }
} // namespace Bess
