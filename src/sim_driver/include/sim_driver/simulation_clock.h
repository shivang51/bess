#pragma once

#include "common/bess_api.h"
#include "common/types.h"

#include <atomic>
#include <cmath>

namespace Bess::SimEngine {

    // A single monotonic virtual clock shared by the simulation engine and all
    // of its drivers. Wall-clock pacing is an engine policy; timestamps inside
    // drivers always come from this clock.
    class BESS_API SimulationClock final {
      public:
        SimulationClock() = default;

        [[nodiscard]] TimeNs now() const noexcept {
            return TimeNs(m_timeNs.load(std::memory_order_acquire));
        }

        void reset(TimeNs time = TimeNs(0)) noexcept {
            const double count = time.count();
            m_timeNs.store(std::isfinite(count) && count > 0.0 ? count : 0.0,
                           std::memory_order_release);
        }

        [[nodiscard]] bool advanceTo(TimeNs time) noexcept {
            const double requested = time.count();
            if (!std::isfinite(requested) || requested < 0.0) {
                return false;
            }

            double current = m_timeNs.load(std::memory_order_acquire);
            while (requested > current) {
                if (m_timeNs.compare_exchange_weak(current,
                                                   requested,
                                                   std::memory_order_acq_rel,
                                                   std::memory_order_acquire)) {
                    return true;
                }
            }

            return requested == current;
        }

      private:
        std::atomic<double> m_timeNs{0.0};
    };

} // namespace Bess::SimEngine
