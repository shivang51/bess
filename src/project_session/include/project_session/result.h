#pragma once

#include "common/bess_api.h"

#include <expected>
#include <string>
#include <utility>

namespace Bess::Session {
    enum class ErrorCode {
        invalidArgument,
        notFound,
        alreadyExists,
        conflict,
        invalidState,
        simulationFailure,
        persistenceFailure,
        transactionFailure,
        rollbackFailure,
    };

    struct BESS_API Error {
        ErrorCode code = ErrorCode::invalidState;
        std::string message;

        static Error invalidArgument(std::string message) {
            return {ErrorCode::invalidArgument, std::move(message)};
        }

        static Error notFound(std::string message) {
            return {ErrorCode::notFound, std::move(message)};
        }

        static Error alreadyExists(std::string message) {
            return {ErrorCode::alreadyExists, std::move(message)};
        }

        static Error conflict(std::string message) {
            return {ErrorCode::conflict, std::move(message)};
        }

        static Error invalidState(std::string message) {
            return {ErrorCode::invalidState, std::move(message)};
        }

        static Error simulationFailure(std::string message) {
            return {ErrorCode::simulationFailure, std::move(message)};
        }

        static Error transactionFailure(std::string message) {
            return {ErrorCode::transactionFailure, std::move(message)};
        }

        static Error rollbackFailure(std::string message) {
            return {ErrorCode::rollbackFailure, std::move(message)};
        }
    };

    template <typename T> using Result = std::expected<T, Error>;
    using Status = Result<void>;

    inline std::unexpected<Error> fail(Error error) {
        return std::unexpected(std::move(error));
    }
} // namespace Bess::Session
