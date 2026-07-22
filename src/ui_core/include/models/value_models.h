#pragma once

#include "models/signal.h"
#include "ui_types.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace Bess::UI {

    template <typename T> struct ValueChange {
        T previous{};
        T value{};
    };

    enum class RangeChangeKind : uint8_t { value, range, step };

    struct RangeChange {
        RangeChangeKind kind = RangeChangeKind::value;
        double previousValue = 0.0;
        double value = 0.0;
        double minimum = 0.0;
        double maximum = 1.0;
        double step = 0.0;
    };

    class BoolModel {
      public:
        using ChangedSignal = Signal<ValueChange<bool>>;

        explicit BoolModel(bool value = false) : m_value(value) {
        }

        [[nodiscard]] bool value() const noexcept {
            return m_value;
        }

        bool setValue(bool value) {
            if (m_value == value) {
                return false;
            }
            const bool previous = m_value;
            m_value = value;
            m_changed.emit({.previous = previous, .value = m_value});
            return true;
        }

        bool toggle() {
            return setValue(!m_value);
        }

        [[nodiscard]] ChangedSignal &changed() noexcept {
            return m_changed;
        }

      private:
        bool m_value = false;
        ChangedSignal m_changed;
    };

    enum class CheckState : uint8_t { unchecked, checked, mixed };

    class CheckStateModel {
      public:
        using ChangedSignal = Signal<ValueChange<CheckState>>;

        explicit CheckStateModel(CheckState value = CheckState::unchecked)
            : m_value(value) {
        }

        [[nodiscard]] CheckState value() const noexcept {
            return m_value;
        }

        bool setValue(CheckState value) {
            if (m_value == value) {
                return false;
            }
            const CheckState previous = m_value;
            m_value = value;
            m_changed.emit({.previous = previous, .value = m_value});
            return true;
        }

        bool toggle(bool cycleMixed = false) {
            switch (m_value) {
            case CheckState::unchecked:
                return setValue(CheckState::checked);
            case CheckState::checked:
                return setValue(cycleMixed ? CheckState::mixed
                                           : CheckState::unchecked);
            case CheckState::mixed:
                return setValue(cycleMixed ? CheckState::unchecked
                                           : CheckState::checked);
            }
            return false;
        }

        [[nodiscard]] ChangedSignal &changed() noexcept {
            return m_changed;
        }

      private:
        CheckState m_value = CheckState::unchecked;
        ChangedSignal m_changed;
    };

    template <typename Id> class SingleSelectionModel {
      public:
        using ChangedSignal = Signal<ValueChange<Id>>;

        explicit SingleSelectionModel(Id value = {}) : m_value(value) {
        }

        [[nodiscard]] Id value() const noexcept {
            return m_value;
        }

        bool select(Id value) {
            if (m_value == value) {
                return false;
            }
            const Id previous = m_value;
            m_value = value;
            m_changed.emit({.previous = previous, .value = m_value});
            return true;
        }

        bool clear() {
            return select({});
        }

        [[nodiscard]] ChangedSignal &changed() noexcept {
            return m_changed;
        }

      private:
        Id m_value;
        ChangedSignal m_changed;
    };

    class RadioGroupModel {
      public:
        using ChangedSignal = Signal<ValueChange<RadioId>>;

        bool registerOption(RadioId id) {
            if (!id || std::find(m_order.begin(), m_order.end(), id) !=
                           m_order.end()) {
                return false;
            }
            m_order.push_back(id);
            return true;
        }

        bool unregisterOption(RadioId id) {
            const auto previousSize = m_order.size();
            std::erase(m_order, id);
            if (m_value == id) {
                static_cast<void>(select({}));
            }
            return m_order.size() != previousSize;
        }

        [[nodiscard]] RadioId value() const noexcept {
            return m_value;
        }

        bool select(RadioId id) {
            if (id && std::find(m_order.begin(), m_order.end(), id) ==
                          m_order.end()) {
                return false;
            }
            if (m_value == id) {
                return false;
            }
            const RadioId previous = m_value;
            m_value = id;
            m_changed.emit({.previous = previous, .value = m_value});
            return true;
        }

        [[nodiscard]] RadioId next(RadioId from, int direction) const noexcept {
            if (m_order.empty() || direction == 0) {
                return {};
            }
            auto it = std::find(m_order.begin(), m_order.end(), from);
            size_t index = it == m_order.end()
                               ? (direction > 0 ? m_order.size() - 1 : 0)
                               : static_cast<size_t>(it - m_order.begin());
            index = direction > 0
                        ? (index + 1) % m_order.size()
                        : (index + m_order.size() - 1) % m_order.size();
            return m_order[index];
        }

        [[nodiscard]] ChangedSignal &changed() noexcept {
            return m_changed;
        }

      private:
        std::vector<RadioId> m_order;
        RadioId m_value;
        ChangedSignal m_changed;
    };

    class RangeModel {
      public:
        using ChangedSignal = Signal<RangeChange>;

        RangeModel(double minimum = 0.0,
                   double maximum = 1.0,
                   double value = 0.0,
                   double step = 0.01) {
            setRangeInternal(minimum, maximum);
            m_step = validStep(step) ? step : 0.0;
            m_value = normalized(value, true);
        }

        [[nodiscard]] double minimum() const noexcept {
            return m_minimum;
        }

        [[nodiscard]] double maximum() const noexcept {
            return m_maximum;
        }

        [[nodiscard]] double value() const noexcept {
            return m_value;
        }

        [[nodiscard]] double step() const noexcept {
            return m_step;
        }

        [[nodiscard]] double normalizedValue() const noexcept {
            const double extent = m_maximum - m_minimum;
            return extent > 0.0 ? (m_value - m_minimum) / extent : 0.0;
        }

        bool setValue(double value, bool snapToStep = true) {
            const double next = normalized(value, snapToStep);
            if (next == m_value) {
                return false;
            }
            const double previous = m_value;
            m_value = next;
            m_changed.emit({.kind = RangeChangeKind::value,
                            .previousValue = previous,
                            .value = m_value,
                            .minimum = m_minimum,
                            .maximum = m_maximum,
                            .step = m_step});
            return true;
        }

        bool setRange(double minimum, double maximum) {
            double nextMinimum = std::isfinite(minimum) ? minimum : 0.0;
            double nextMaximum = std::isfinite(maximum) ? maximum : 1.0;
            if (nextMaximum < nextMinimum) {
                std::swap(nextMinimum, nextMaximum);
            }
            if (nextMinimum == m_minimum && nextMaximum == m_maximum) {
                return false;
            }
            const double previous = m_value;
            m_minimum = nextMinimum;
            m_maximum = nextMaximum;
            m_value = normalized(m_value, true);
            m_changed.emit({.kind = RangeChangeKind::range,
                            .previousValue = previous,
                            .value = m_value,
                            .minimum = m_minimum,
                            .maximum = m_maximum,
                            .step = m_step});
            return true;
        }

        bool setStep(double step) {
            const double next = validStep(step) ? step : 0.0;
            if (next == m_step) {
                return false;
            }
            const double previous = m_value;
            m_step = next;
            m_value = normalized(m_value, true);
            m_changed.emit({.kind = RangeChangeKind::step,
                            .previousValue = previous,
                            .value = m_value,
                            .minimum = m_minimum,
                            .maximum = m_maximum,
                            .step = m_step});
            return true;
        }

        [[nodiscard]] ChangedSignal &changed() noexcept {
            return m_changed;
        }

      private:
        [[nodiscard]] static bool validStep(double value) noexcept {
            return std::isfinite(value) && value > 0.0;
        }

        void setRangeInternal(double minimum, double maximum) noexcept {
            m_minimum = std::isfinite(minimum) ? minimum : 0.0;
            m_maximum = std::isfinite(maximum) ? maximum : 1.0;
            if (m_maximum < m_minimum) {
                std::swap(m_minimum, m_maximum);
            }
        }

        [[nodiscard]] double normalized(double value,
                                        bool snapToStep) const noexcept {
            if (!std::isfinite(value)) {
                value = m_minimum;
            }
            value = std::clamp(value, m_minimum, m_maximum);
            if (snapToStep && validStep(m_step) && m_maximum > m_minimum) {
                const double steps = std::round((value - m_minimum) / m_step);
                value = m_minimum + steps * m_step;
                value = std::clamp(value, m_minimum, m_maximum);
            }
            return value;
        }

        double m_minimum = 0.0;
        double m_maximum = 1.0;
        double m_value = 0.0;
        double m_step = 0.01;
        ChangedSignal m_changed;
    };

} // namespace Bess::UI
