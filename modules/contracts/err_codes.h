#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace kvdb::err_codes {
    using Code = std::string_view;

    inline constexpr std::size_t CodeLength = 6;

    enum class Group : std::uint8_t
    {
        Unknown = 0,
        Shared,
        ContractsUnsupportedTypes,
        ContractsInvalidTypeComposition,
        ContractsInvalidValue,
        Engine,
        QueryParser,
        ResponseConstructor,
        Server
    };

    [[nodiscard]] constexpr bool isDigit(char ch) noexcept {
        return ch >= '0' && ch <= '9';
    }

    [[nodiscard]] constexpr bool startsWith(Code value, Code prefix) noexcept {
        if (prefix.size() > value.size()) {
            return false;
        }

        for (std::size_t i = 0; i < prefix.size(); ++i) {
            if (value[i] != prefix[i]) {
                return false;
            }
        }

        return true;
    }

    [[nodiscard]] constexpr bool hasValidShape(Code code) noexcept {
        if (code.size() != CodeLength) {
            return false;
        }

        for (char ch : code) {
            if (!isDigit(ch)) {
                return false;
            }
        }

        return true;
    }

    [[nodiscard]] constexpr Group groupOf(Code code) noexcept {
        if (!hasValidShape(code)) {
            return Group::Unknown;
        }

        if (startsWith(code, "000")) {
            return Group::Shared;
        }

        if (startsWith(code, "011")) {
            return Group::ContractsUnsupportedTypes;
        }

        if (startsWith(code, "012")) {
            return Group::ContractsInvalidTypeComposition;
        }

        if (startsWith(code, "013")) {
            return Group::ContractsInvalidValue;
        }

        if (startsWith(code, "020")) {
            return Group::Engine;
        }

        if (startsWith(code, "030")) {
            return Group::QueryParser;
        }

        if (startsWith(code, "040")) {
            return Group::ResponseConstructor;
        }

        if (startsWith(code, "050")) {
            return Group::Server;
        }

        return Group::Unknown;
    }

    [[nodiscard]] constexpr bool isShared(Code code) noexcept {
        return groupOf(code) == Group::Shared;
    }

    [[nodiscard]] constexpr bool isContractsError(Code code) noexcept {
        const auto group = groupOf(code);
        return group == Group::ContractsUnsupportedTypes
            || group == Group::ContractsInvalidTypeComposition
            || group == Group::ContractsInvalidValue;
    }

    [[nodiscard]] constexpr bool isEngineError(Code code) noexcept {
        return groupOf(code) == Group::Engine;
    }

    [[nodiscard]] constexpr bool isQueryParserError(Code code) noexcept {
        return groupOf(code) == Group::QueryParser;
    }

    [[nodiscard]] constexpr bool isResponseConstructorError(Code code) noexcept {
        return groupOf(code) == Group::ResponseConstructor;
    }

    [[nodiscard]] constexpr bool isServerError(Code code) noexcept {
        return groupOf(code) == Group::Server;
    }

    namespace shared {
        inline constexpr Code NotImplemented = "000001";
        inline constexpr Code UnhandledException = "000002";
        inline constexpr Code ProgramBug = "000003";
    }

    namespace contracts::unsupported_types {
        inline constexpr Code NullableKeyType = "011001";
        inline constexpr Code ArrayKeyType = "011002";
    }

    namespace contracts::invalid_type_composition {
        inline constexpr Code NestedNullableType = "012001";
        inline constexpr Code NestedArrayType = "012002";
    }

    namespace contracts::invalid_values {
        inline constexpr Code InvalidCharSeqLength = "013001";
        inline constexpr Code InvalidIntByteCount = "013002";
        inline constexpr Code InvalidUIntByteCount = "013003";
        inline constexpr Code InvalidTableName = "013004";
    }

    namespace engine {
        inline constexpr Code GetNotFound = "020001";
        inline constexpr Code DelNotFound = "020002";
        inline constexpr Code KeyTypeConflict = "020003";
    }

    namespace query_parser {
        inline constexpr Code Generic = "030000";
    }

    namespace response_constructor {
        inline constexpr Code Generic = "040000";
    }

    namespace server {
        inline constexpr Code Generic = "050000";
    }
}
