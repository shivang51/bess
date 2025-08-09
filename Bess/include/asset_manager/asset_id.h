#pragma once
#include <cstdint>
#include <string_view>

namespace Bess::Assets {
    constexpr uint64_t FNV_OFFSET_BASIS = 0xcbf29ce484222325;
    constexpr uint64_t FNV_PRIME = 0x100000001b3;

    constexpr uint64_t fnv1a_hash_step(uint64_t current_hash, const std::string_view str) {
        for (char c : str) {
            current_hash ^= static_cast<uint64_t>(c);
            current_hash *= FNV_PRIME;
        }
        return current_hash;
    }

    template <typename... TStrs>
    constexpr uint64_t fnv1a_hash_combined_recursive(uint64_t current_hash, const std::string_view first, TStrs... rest) {
        uint64_t hash_after_first = fnv1a_hash_step(current_hash, first);
        if constexpr (sizeof...(rest) > 0) {
            hash_after_first ^= static_cast<uint64_t>('\0');
            hash_after_first *= FNV_PRIME;
            return fnv1a_hash_combined_recursive(hash_after_first, rest...);
        }
        return hash_after_first;
    }

    template <typename... TStrs>
    constexpr uint64_t fnv1a_hash_combined(TStrs... strs) {
        return fnv1a_hash_combined_recursive(FNV_OFFSET_BASIS, strs...);
    }

    template <typename TAsset, size_t N>
    class AssetID {
      public:
        const uint64_t id;
        const std::array<std::string_view, N> paths;

        template <typename... TPaths>
        constexpr explicit AssetID(TPaths... assetPaths)
            : id(fnv1a_hash_combined(assetPaths...)), paths{assetPaths...} {
            static_assert(sizeof...(assetPaths) == N, "Internal error: Number of paths does not match template argument N");
        }

        constexpr bool operator==(const AssetID &other) const {
            return id == other.id;
        }

        constexpr bool operator!=(const AssetID &other) const {
            return id != other.id;
        }
    };
}