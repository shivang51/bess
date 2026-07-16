#include "common/bess_api.h"

#include <cstdint>

extern "C" BESS_RUNTIME_API std::uint32_t bessRuntimeAbiVersion() noexcept {
    return 1;
}
