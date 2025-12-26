#include "component_definition.h"
#include "types.h"
#include <logger.h>
#include <memory>
#include <type_traits>

template <>
std::atomic_int Bess::TypeMap<std::shared_ptr<Bess::SimEngine::Trait>>::LastTypeId(0);

namespace Bess::SimEngine {
    // --- hashing helpers (FNV-1a 64-bit) ---
    namespace {
        constexpr uint64_t FNV_OFFSET_BASIS_64 = 1469598103934665603ull;
        constexpr uint64_t FNV_PRIME_64 = 1099511628211ull;

        inline uint64_t fnv1aBytes(uint64_t hash, const uint8_t *data, size_t len) noexcept {
            for (size_t i = 0; i < len; ++i) {
                hash ^= static_cast<uint64_t>(data[i]);
                hash *= FNV_PRIME_64;
            }
            return hash;
        }

        template <typename T>
        inline uint64_t fnv1aPod(uint64_t hash, const T &value) noexcept {
            static_assert(std::is_trivially_copyable_v<T>, "fnv1aPod requires trivially copyable type");
            return fnv1aBytes(hash, reinterpret_cast<const uint8_t *>(&value), sizeof(T));
        }

        inline uint64_t fnv1aString(uint64_t hash, const std::string &s) noexcept {
            return fnv1aBytes(hash, reinterpret_cast<const uint8_t *>(s.data()), s.size());
        }

        template <typename E>
        inline uint64_t fnv1aEnum(uint64_t hash, E e) noexcept {
            using U = std::underlying_type_t<E>;
            U v = static_cast<U>(e);
            return fnv1aPod(hash, v);
        }

        inline uint64_t hashPinDetails(uint64_t hash, const PinDetail &pin) noexcept {
            hash = fnv1aEnum(hash, pin.type);
            hash = fnv1aString(hash, pin.name);
            hash = fnv1aEnum(hash, pin.extendedType);
            return hash;
        }

        template <typename T>
        inline uint64_t hashVector(uint64_t hash, const std::vector<T> &vec, auto elemHasher) noexcept {
            hash = fnv1aPod(hash, vec.size());
            for (const auto &elem : vec) {
                hash = elemHasher(hash, elem);
                // separator byte to reduce accidental concatenation collisions
                const uint8_t sep = 0xff;
                hash = fnv1aBytes(hash, &sep, 1);
            }
            return hash;
        }
    } // namespace

    bool ComponentDefinition::onSlotsResizeReq(SlotsGroupType groupType, size_t newSize) {
        if (groupType == SlotsGroupType::input)
            return m_inputSlotsInfo.isResizeable;
        else
            return m_outputSlotsInfo.isResizeable;
    }

    uint64_t ComponentDefinition::getHash() const {
        return m_hash;
    }

    void ComponentDefinition::computeHash() {
        uint64_t hash = FNV_OFFSET_BASIS_64;

        hash = fnv1aString(hash, m_name);
        hash = fnv1aString(hash, m_groupName);
        if (!m_inputSlotsInfo.isResizeable) {
            hash = fnv1aPod(hash, m_inputSlotsInfo.count);
        }

        if (!m_outputSlotsInfo.isResizeable) {
            hash = fnv1aPod(hash, m_outputSlotsInfo.count);
        }

        if (m_opInfo.op != '0') {
            hash = fnv1aPod(hash, m_opInfo.op);
            hash = fnv1aPod(hash, m_opInfo.shouldNegateOutput);
        }

        hash = fnv1aPod(hash, m_simDelay.count());

        m_hash = hash;
    }

    SimTime ComponentDefinition::getSimDelay() {
        return m_simDelay;
    }

    SimTime ComponentDefinition::getRescheduleDelay() {
        if (!m_shouldAutoReschedule) {
            BESS_SE_ERROR("[ComponentDefinition] For comp {}, getRescheduleDelay called on component that should not auto-reschedule", m_name);
        }
        return getSimDelay();
    }
} // namespace Bess::SimEngine
