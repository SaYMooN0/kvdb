#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

namespace kvdb::contracts {
    struct Err
    {
        virtual ~Err() = default;

        [[nodiscard]]
        virtual std::string_view module() const noexcept = 0;

        [[nodiscard]]
        virtual std::string_view code() const noexcept = 0;

        [[nodiscard]]
        virtual std::string_view message() const noexcept = 0;
    };

    struct ContractsErr : Err
    {
        [[nodiscard]]
        std::string_view module() const noexcept final {
            return "contracts";
        }
    };

    struct NullableKeyTypeErr final : ContractsErr
    {
        [[nodiscard]]
        std::string_view code() const noexcept override {
            return "NullableKeyType";
        }

        [[nodiscard]]
        std::string_view message() const noexcept override {
            return "Key type cannot be nullable.";
        }
    };

    struct ArrayKeyTypeErr final : ContractsErr
    {
        [[nodiscard]]
        std::string_view code() const noexcept override {
            return "ArrayKeyType";
        }

        [[nodiscard]]
        std::string_view message() const noexcept override {
            return "Key type cannot be an array.";
        }
    };

    struct NestedNullableTypeErr final : ContractsErr
    {
        [[nodiscard]]
        std::string_view code() const noexcept override {
            return "NestedNullableType";
        }

        [[nodiscard]]
        std::string_view message() const noexcept override {
            return "Nested nullable types are not allowed.";
        }
    };

    struct NestedArrayTypeErr final : ContractsErr
    {
        [[nodiscard]]
        std::string_view code() const noexcept override {
            return "NestedArrayType";
        }

        [[nodiscard]]
        std::string_view message() const noexcept override {
            return "Nested array types are not allowed.";
        }
    };

    struct InvalidCharSeqLengthErr final : ContractsErr
    {
        std::uint16_t length = 0;

        explicit InvalidCharSeqLengthErr(const std::uint16_t length)
            : length(length) {
            messageValue = "Invalid charseq length: " + std::to_string(length) + ".";
        }

        [[nodiscard]]
        std::string_view code() const noexcept override {
            return "InvalidCharSeqLength";
        }

        [[nodiscard]]
        std::string_view message() const noexcept override {
            return messageValue;
        }

    private:
        std::string messageValue;
    };

    struct InvalidIntByteCountErr final : ContractsErr
    {
        std::uint16_t byteCount = 0;

        explicit InvalidIntByteCountErr(const std::uint16_t byteCount)
            : byteCount(byteCount) {
            messageValue = "Invalid int byte count: " + std::to_string(byteCount) + ".";
        }

        [[nodiscard]]
        std::string_view code() const noexcept override {
            return "InvalidIntByteCount";
        }

        [[nodiscard]]
        std::string_view message() const noexcept override {
            return messageValue;
        }

    private:
        std::string messageValue;
    };

    struct InvalidUIntByteCountErr final : ContractsErr
    {
        std::uint16_t byteCount = 0;

        explicit InvalidUIntByteCountErr(const std::uint16_t byteCount)
            : byteCount(byteCount) {
            messageValue = "Invalid uint byte count: " + std::to_string(byteCount) + ".";
        }

        [[nodiscard]]
        std::string_view code() const noexcept override {
            return "InvalidUIntByteCount";
        }

        [[nodiscard]]
        std::string_view message() const noexcept override {
            return messageValue;
        }

    private:
        std::string messageValue;
    };

    struct InvalidTableNameErr final : ContractsErr
    {
        std::string tableName;
        std::string reason;

        InvalidTableNameErr(std::string tableName, std::string reason)
            : tableName(std::move(tableName)),
              reason(std::move(reason)) {
            messageValue = "Invalid table name: '" + this->tableName + "'. " + this->reason;
        }

        [[nodiscard]]
        std::string_view code() const noexcept override {
            return "InvalidTableName";
        }

        [[nodiscard]]
        std::string_view message() const noexcept override {
            return messageValue;
        }

    private:
        std::string messageValue;
    };

    struct ParserReturnedNullCmdErr final : ContractsErr
    {
        [[nodiscard]]
        std::string_view code() const noexcept override {
            return "ParserReturnedNullCmd";
        }

        [[nodiscard]]
        std::string_view message() const noexcept override {
            return "Parser returned null command.";
        }
    };

    struct ParserReturnedNullErr final : ContractsErr
    {
        [[nodiscard]]
        std::string_view code() const noexcept override {
            return "ParserReturnedNullErr";
        }

        [[nodiscard]]
        std::string_view message() const noexcept override {
            return "Parser returned null error.";
        }
    };

    struct EngineReturnedNullErr final : ContractsErr
    {
        [[nodiscard]]
        std::string_view code() const noexcept override {
            return "EngineReturnedNullErr";
        }

        [[nodiscard]]
        std::string_view message() const noexcept override {
            return "Engine returned null error.";
        }
    };
}
