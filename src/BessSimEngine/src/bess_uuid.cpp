#include "bess_uuid.h"
#include <random>

namespace Bess {
    // thanks to Mr. Cherno
    static std::random_device s_RandomDevice;
    static std::mt19937_64 s_Engine(s_RandomDevice());
    static std::uniform_int_distribution<uint64_t> s_UniformDistribution;

    UUID UUID::null = 0;

    UUID::UUID() {
        m_UUID = s_UniformDistribution(s_Engine);
    }

    UUID::UUID(uint64_t uuid) {
        m_UUID = uuid;
    }
} // namespace Bess
