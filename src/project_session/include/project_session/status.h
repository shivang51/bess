#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>

namespace Bess {
    using StateId = std::uint64_t;

    inline constexpr std::size_t NoOp = std::numeric_limits<std::size_t>::max();

    enum class Err : std::uint8_t {
        none = 0,
        badArg,
        empty,
        invalid,
        apply,
        undo,
        redo,
        conflict,
        busy,
        faulted,
        noPath,
        notFound,
        notFile,
        tooLarge,
        io,
        parse,
        schema,
        rollback,
        except,
    };

    class Status {
      public:
        Status() = default;

        [[nodiscard]] static Status ok() {
            return {};
        }

        [[nodiscard]] static Status fail(Err err, std::string msg) {
            return Status(err, std::move(msg));
        }

        [[nodiscard]] bool isOk() const noexcept {
            return m_err == Err::none;
        }

        [[nodiscard]] explicit operator bool() const noexcept {
            return isOk();
        }

        [[nodiscard]] Err err() const noexcept {
            return m_err;
        }

        [[nodiscard]] const std::string &msg() const noexcept {
            return m_msg;
        }

      private:
        Status(Err err, std::string msg)
            : m_err(err == Err::none ? Err::badArg : err),
              m_msg(std::move(msg)) {
        }

        Err m_err = Err::none;
        std::string m_msg;
    };

    struct TxResult {
        Status status;
        StateId before = 0;
        StateId after = 0;
        std::size_t op = NoOp;

        [[nodiscard]] explicit operator bool() const noexcept {
            return status.isOk();
        }

        [[nodiscard]] bool changed() const noexcept {
            return status.isOk() && before != after;
        }
    };

    template <typename T> struct ValResult {
        Status status;
        T val{};

        [[nodiscard]] explicit operator bool() const noexcept {
            return status.isOk();
        }
    };
} // namespace Bess
