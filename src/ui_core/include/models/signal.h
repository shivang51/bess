#pragma once

#include "common/types.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

namespace Bess::UI {

    // Small single-threaded signal used by UI models. Connections are RAII,
    // callbacks may safely connect/disconnect while a notification is being
    // emitted, and a connection may outlive its signal.
    template <typename Event> class Signal {
      private:
        struct State {
            HashMap<uint64_t, std::function<void(const Event &)>> callbacks;
            uint64_t nextId = 1;
        };

      public:
        class Connection {
          public:
            Connection() = default;
            Connection(const Connection &) = delete;
            Connection &operator=(const Connection &) = delete;

            Connection(Connection &&other) noexcept
                : m_state(std::move(other.m_state)),
                  m_id(std::exchange(other.m_id, 0)) {
            }

            Connection &operator=(Connection &&other) noexcept {
                if (this != &other) {
                    disconnect();
                    m_state = std::move(other.m_state);
                    m_id = std::exchange(other.m_id, 0);
                }
                return *this;
            }

            ~Connection() {
                disconnect();
            }

            void disconnect() noexcept {
                if (m_id == 0) {
                    return;
                }
                if (const auto state = m_state.lock()) {
                    state->callbacks.erase(m_id);
                }
                m_state.reset();
                m_id = 0;
            }

            [[nodiscard]] bool connected() const noexcept {
                const auto state = m_state.lock();
                return m_id != 0 && state != nullptr &&
                       state->callbacks.contains(m_id);
            }

          private:
            friend class Signal;
            Connection(std::weak_ptr<State> state, uint64_t id)
                : m_state(std::move(state)),
                  m_id(id) {
            }

            std::weak_ptr<State> m_state;
            uint64_t m_id = 0;
        };

        using Callback = std::function<void(const Event &)>;

        Signal() : m_state(std::make_shared<State>()) {
        }
        Signal(const Signal &) = delete;
        Signal &operator=(const Signal &) = delete;
        Signal(Signal &&) noexcept = default;
        Signal &operator=(Signal &&) noexcept = default;

        [[nodiscard]] Connection connect(Callback callback) {
            if (!callback) {
                return {};
            }
            const uint64_t id = m_state->nextId++;
            m_state->callbacks.emplace(id, std::move(callback));
            return Connection{m_state, id};
        }

        void emit(const Event &event) const {
            std::vector<uint64_t> snapshot;
            snapshot.reserve(m_state->callbacks.size());
            for (const auto &[id, callback] : m_state->callbacks) {
                (void)callback;
                snapshot.push_back(id);
            }
            for (const auto id : snapshot) {
                const auto it = m_state->callbacks.find(id);
                if (it != m_state->callbacks.end()) {
                    // Copy before invocation: the callback may disconnect and
                    // destroy its own storage.
                    auto callback = it->second;
                    callback(event);
                }
            }
        }

        [[nodiscard]] size_t connectionCount() const noexcept {
            return m_state->callbacks.size();
        }

      private:
        std::shared_ptr<State> m_state;
    };

} // namespace Bess::UI
