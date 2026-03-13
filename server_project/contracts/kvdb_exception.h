#pragma once

#include <exception>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "err_codes.h"

namespace kvdb {
    class KvdbException final : public std::exception {
    public:
        [[nodiscard]] const char* what() const noexcept override {
            return what_.c_str();
        }

        [[nodiscard]] const std::string& code() const noexcept {
            return code_;
        }

        [[nodiscard]] const std::string& msg() const noexcept {
            return msg_;
        }

        [[nodiscard]] const std::optional<std::string>& fixRecommendation() const noexcept {
            return fixRecommendation_;
        }

        [[nodiscard]] const std::string& details() const noexcept {
            return details_;
        }

        [[nodiscard]] err_codes::Group group() const noexcept {
            return err_codes::groupOf(code_);
        }

        [[nodiscard]] bool isShared() const noexcept {
            return err_codes::isShared(code_);
        }

        [[nodiscard]] bool isContractsError() const noexcept {
            return err_codes::isContractsError(code_);
        }

        [[nodiscard]] bool isEngineError() const noexcept {
            return err_codes::isEngineError(code_);
        }

        [[nodiscard]] bool isQueryParserError() const noexcept {
            return err_codes::isQueryParserError(code_);
        }

        [[nodiscard]] bool isResponseConstructorError() const noexcept {
            return err_codes::isResponseConstructorError(code_);
        }

        [[nodiscard]] bool isServerError() const noexcept {
            return err_codes::isServerError(code_);
        }

    public:
        // shared
        [[nodiscard]] static KvdbException notImplemented(
            std::string details = {},
            std::optional<std::string> fixRecommendation =
                std::string("Implement the requested functionality.")
        ) {
            return make(
                err_codes::shared::NotImplemented,
                "Not implemented.",
                std::move(details),
                std::move(fixRecommendation)
            );
        }

        [[nodiscard]] static KvdbException unhandledException(
            std::string details = {},
            std::optional<std::string> fixRecommendation =
                std::string("Catch the exception closer to the source and convert it into a supported error.")
        ) {
            return make(
                err_codes::shared::UnhandledException,
                "Unhandled exception.",
                std::move(details),
                std::move(fixRecommendation)
            );
        }

        [[nodiscard]] static KvdbException programBug(
            std::string details = {},
            std::optional<std::string> fixRecommendation =
                std::string("Check invariants and internal state transitions.")
        ) {
            return make(
                err_codes::shared::ProgramBug,
                "Program bug or unexpected state.",
                std::move(details),
                std::move(fixRecommendation)
            );
        }

        // contracts: unsupported types
        [[nodiscard]] static KvdbException nullableKeyType(
            std::string details = {},
            std::optional<std::string> fixRecommendation =
                std::string("Use a non-nullable primitive key type: uuid, charseq(n), int(n), uint(n), bool.")
        ) {
            return make(
                err_codes::contracts::unsupported_types::NullableKeyType,
                "Nullable key type is unsupported.",
                std::move(details),
                std::move(fixRecommendation)
            );
        }

        [[nodiscard]] static KvdbException arrayKeyType(
            std::string details = {},
            std::optional<std::string> fixRecommendation =
                std::string("Use a non-array primitive key type: uuid, charseq(n), int(n), uint(n), bool.")
        ) {
            return make(
                err_codes::contracts::unsupported_types::ArrayKeyType,
                "Array key type is unsupported.",
                std::move(details),
                std::move(fixRecommendation)
            );
        }

        // contracts: invalid type composition
        [[nodiscard]] static KvdbException nestedNullableType(
            std::string details = {},
            std::optional<std::string> fixRecommendation =
                std::string("Do not use nullable(nullable(x)). If needed, separate wrappers with array(...), for example nullable(array(nullable(bool))).")
        ) {
            return make(
                err_codes::contracts::invalid_type_composition::NestedNullableType,
                "Nested nullable type is unsupported.",
                std::move(details),
                std::move(fixRecommendation)
            );
        }

        [[nodiscard]] static KvdbException nestedArrayType(
            std::string details = {},
            std::optional<std::string> fixRecommendation =
                std::string("Do not place array(...) inside another array(...) at any depth.")
        ) {
            return make(
                err_codes::contracts::invalid_type_composition::NestedArrayType,
                "Nested array type is unsupported.",
                std::move(details),
                std::move(fixRecommendation)
            );
        }

        // contracts: invalid values
        [[nodiscard]] static KvdbException invalidCharSeqLength(
            std::string details = {},
            std::optional<std::string> fixRecommendation =
                std::string("Use charseq(n) where n is in range 1..65535.")
        ) {
            return make(
                err_codes::contracts::invalid_values::InvalidCharSeqLength,
                "Invalid charseq length.",
                std::move(details),
                std::move(fixRecommendation)
            );
        }

        [[nodiscard]] static KvdbException invalidIntByteCount(
            std::string details = {},
            std::optional<std::string> fixRecommendation =
                std::string("Use int(n) where n is in range 1..8.")
        ) {
            return make(
                err_codes::contracts::invalid_values::InvalidIntByteCount,
                "Invalid int byte count.",
                std::move(details),
                std::move(fixRecommendation)
            );
        }

        [[nodiscard]] static KvdbException invalidUIntByteCount(
            std::string details = {},
            std::optional<std::string> fixRecommendation =
                std::string("Use uint(n) where n is in range 1..8.")
        ) {
            return make(
                err_codes::contracts::invalid_values::InvalidUIntByteCount,
                "Invalid uint byte count.",
                std::move(details),
                std::move(fixRecommendation)
            );
        }

        [[nodiscard]] static KvdbException invalidTableName(
            std::string details = {},
            std::optional<std::string> fixRecommendation =
                std::string("Use 2..127 characters: Russian or English letters, digits, '_' or '-'.")
        ) {
            return make(
                err_codes::contracts::invalid_values::InvalidTableName,
                "Invalid table name.",
                std::move(details),
                std::move(fixRecommendation)
            );
        }

        // engine
        [[nodiscard]] static KvdbException getNotFound(
            std::string details = {},
            std::optional<std::string> fixRecommendation =
                std::string("Check whether the key exists before calling get.")
        ) {
            return make(
                err_codes::engine::GetNotFound,
                "Key not found on get.",
                std::move(details),
                std::move(fixRecommendation)
            );
        }

        [[nodiscard]] static KvdbException delNotFound(
            std::string details = {},
            std::optional<std::string> fixRecommendation =
                std::string("Check whether the key exists before calling del.")
        ) {
            return make(
                err_codes::engine::DelNotFound,
                "Key not found on del.",
                std::move(details),
                std::move(fixRecommendation)
            );
        }

        [[nodiscard]] static KvdbException keyTypeConflict(
            std::string details = {},
            std::optional<std::string> fixRecommendation =
                std::string("Use a key value matching the table key type.")
        ) {
            return make(
                err_codes::engine::KeyTypeConflict,
                "Key type conflict.",
                std::move(details),
                std::move(fixRecommendation)
            );
        }

        // generic module factories
        [[nodiscard]] static KvdbException queryParserError(
            std::string msg,
            std::string details = {},
            std::optional<std::string> fixRecommendation = std::nullopt
        ) {
            return make(
                err_codes::query_parser::Generic,
                std::move(msg),
                std::move(details),
                std::move(fixRecommendation)
            );
        }

        [[nodiscard]] static KvdbException responseConstructorError(
            std::string msg,
            std::string details = {},
            std::optional<std::string> fixRecommendation = std::nullopt
        ) {
            return make(
                err_codes::response_constructor::Generic,
                std::move(msg),
                std::move(details),
                std::move(fixRecommendation)
            );
        }

        [[nodiscard]] static KvdbException serverError(
            std::string msg,
            std::string details = {},
            std::optional<std::string> fixRecommendation = std::nullopt
        ) {
            return make(
                err_codes::server::Generic,
                std::move(msg),
                std::move(details),
                std::move(fixRecommendation)
            );
        }

    private:
        KvdbException(
            std::string code,
            std::string msg,
            std::optional<std::string> fixRecommendation,
            std::string details
        )
            : code_(std::move(code)),
              msg_(std::move(msg)),
              fixRecommendation_(std::move(fixRecommendation)),
              details_(std::move(details)),
              what_(buildWhat(code_, msg_, fixRecommendation_, details_)) {
        }

        [[nodiscard]] static KvdbException make(
            err_codes::Code code,
            std::string msg,
            std::string details = {},
            std::optional<std::string> fixRecommendation = std::nullopt
        ) {
            return KvdbException(
                std::string(code),
                std::move(msg),
                std::move(fixRecommendation),
                std::move(details)
            );
        }

        [[nodiscard]] static std::string buildWhat(
            const std::string& code,
            const std::string& msg,
            const std::optional<std::string>& fixRecommendation,
            const std::string& details
        ) {
            std::string result = "[" + code + "] " + msg;

            if (!details.empty()) {
                result += " Details: " + details;
            }

            if (fixRecommendation.has_value() && !fixRecommendation->empty()) {
                result += " Fix: " + *fixRecommendation;
            }

            return result;
        }

    private:
        std::string code_;
        std::string msg_;
        std::optional<std::string> fixRecommendation_;
        std::string details_;
        std::string what_;
    };
}