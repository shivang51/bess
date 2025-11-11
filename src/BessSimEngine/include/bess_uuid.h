#pragma once

#include "bess_api.h"
#include <cstdint>
#include <functional>

namespace Bess {
    class BESS_API UUID {
      public:
        static UUID null;

        UUID();
        UUID(uint64_t id);
        UUID(const UUID &other) = default;

        operator uint64_t() const { return m_UUID; }

        bool operator==(const Bess::UUID &other) const {
            return other.m_UUID == m_UUID;
        }

      private:
        uint64_t m_UUID;
    };
} // namespace Bess

namespace std {
    template <>
    struct hash<Bess::UUID> {
        std::size_t operator()(const Bess::UUID &uuid) const noexcept {
            return std::hash<uint64_t>()(static_cast<uint64_t>(uuid));
        }
    };
} // namespace std
