#pragma once

#include "bess_api.h"
#include "json/json.h"
#include <cstdint>
#include <functional>

namespace Bess {
    class BESS_API UUID {
      public:
        constexpr UUID(uint64_t id) noexcept : m_UUID(id) {}

        UUID();
        ~UUID() = default;

        constexpr UUID(const UUID &other) noexcept = default;

        constexpr operator uint64_t() const noexcept { return m_UUID; }

        constexpr bool operator==(const UUID &other) const noexcept {
            return m_UUID == other.m_UUID;
        }

        constexpr bool operator!=(const UUID &other) const noexcept {
            return m_UUID != other.m_UUID;
        }

        static const UUID null;

      private:
        uint64_t m_UUID;
    };

    namespace JsonConvert {
        BESS_API void toJsonValue(const Bess::UUID &uuid, Json::Value &j);
        BESS_API void fromJsonValue(const Json::Value &j, Bess::UUID &uuid);
    } // namespace JsonConvert
} // namespace Bess

namespace std {
    template <>
    struct hash<Bess::UUID> {
        std::size_t operator()(const Bess::UUID &uuid) const noexcept {
            return std::hash<uint64_t>()(static_cast<uint64_t>(uuid));
        }
    };
} // namespace std
