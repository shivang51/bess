#include "../include/component_definition.h"
#include "../include/types.h"
#include <logger.h>
#include <type_traits>

namespace Bess::SimEngine {
    ComponentDefinition::ComponentDefinition(
        ComponentType type,
        const std::string &name,
        const std::string &category,
        int inputCount, int outputCount,
        SimulationFunction simFunction,
        SimDelayNanoSeconds delay, char op) {
        this->type = type;
        this->name = name;
        this->category = category;
        this->simulationFunction = simFunction;
        this->inputCount = inputCount;
        this->outputCount = outputCount;
        this->delay = delay;
        this->op = op;
        this->auxData = getExpressions(inputCount);
    }

    ComponentDefinition::ComponentDefinition(
        ComponentType type,
        const std::string &name,
        const std::string &category,
        int inputCount, int outputCount,
        SimulationFunction simFunction,
        SimDelayNanoSeconds delay, const std::vector<std::string> &expr) {
        this->type = type;
        this->name = name;
        this->category = category;
        this->simulationFunction = simFunction;
        this->inputCount = inputCount;
        this->outputCount = outputCount;
        this->delay = delay;
        this->expressions = expr;
        this->auxData = expressions;
    }

    const Properties::ModifiableProperties &ComponentDefinition::getModifiableProperties() const {
        return m_modifiableProperties;
    }

    std::vector<std::string> ComponentDefinition::getExpressions(int inputCount) const {
        if (op == '0') {
            return expressions;
        }

        if (inputCount == -1) {
            BESS_SE_WARN("[SimulationEngine][ComponentDefinition] Input count not provided for expression generation");
            return {};
        }

        if (inputCount != 1 && outputCount == 1) {
            std::string expr = negate ? "!(0" : "0";
            for (size_t i = 1; i < inputCount; i++) {
                expr += op + std::to_string(i);
            }
            if (negate)
                expr += ")";
            return {expr};
        } else if (inputCount == outputCount) {
            std::vector<std::string> expr;
            expr.reserve(inputCount);
            for (int i = 0; i < inputCount; i++) {
                expr.emplace_back(std::format("{}{}", op, i));
            }
            return expr;
        }

        BESS_SE_ERROR("Invalid IO config for expression generation");
        assert(false);
    }

    std::pair<std::span<const PinDetails>, std::span<const PinDetails>> ComponentDefinition::getPinDetails() const {
        std::span<const PinDetails> in, out;

        if (inputPinDetails.size() == inputCount) {
            in = inputPinDetails;
        }
        if (outputPinDetails.size() == outputCount) {
            out = outputPinDetails;
        }

        return {in, out};
    }

    ComponentDefinition &ComponentDefinition::addModifiableProperty(Properties::ComponentProperty property, std::any value) {
        m_modifiableProperties[property].emplace_back(value);
        return *this;
    }

    ComponentDefinition &ComponentDefinition::addModifiableProperty(Properties::ComponentProperty property, const std::vector<std::any> &value) {
        m_modifiableProperties[property] = value;
        return *this;
    }

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

        inline uint64_t hashPinDetails(uint64_t hash, const PinDetails &pin) noexcept {
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

    uint64_t ComponentDefinition::getHash() const noexcept {
        if (m_hashComputed) {
            return m_cachedHash;
        }

        uint64_t hash = FNV_OFFSET_BASIS_64;

        hash = fnv1aEnum(hash, type);
        hash = fnv1aString(hash, name);
        hash = fnv1aString(hash, category);

        const auto delayCount = delay.count();
        const auto setupCount = setupTime.count();
        const auto holdCount = holdTime.count();
        hash = fnv1aPod(hash, delayCount);
        hash = fnv1aPod(hash, setupCount);
        hash = fnv1aPod(hash, holdCount);

        hash = fnv1aPod(hash, inputCount);
        hash = fnv1aPod(hash, outputCount);
        hash = fnv1aPod(hash, op);
        hash = fnv1aPod(hash, negate);

        hash = hashVector(hash, expressions, [](uint64_t h, const std::string &s) noexcept {
            return fnv1aString(h, s);
        });

        hash = hashVector(hash, inputPinDetails, [](uint64_t h, const PinDetails &p) noexcept {
            return hashPinDetails(h, p);
        });
        hash = hashVector(hash, outputPinDetails, [](uint64_t h, const PinDetails &p) noexcept {
            return hashPinDetails(h, p);
        });

        m_cachedHash = hash;
        m_hashComputed = true;
        return m_cachedHash;
    }
} // namespace Bess::SimEngine
