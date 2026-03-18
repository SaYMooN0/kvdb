#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace kvdb::contracts {
    struct UuidCmdValue final
    {
        std::array<std::uint8_t, 16> bytes{};
    };

    struct CharSeqCmdValue final
    {
        std::string utf8Value;
    };

    // The bytes vector stores the absolute value in big-endian order.
    // Zero should be represented as a single zero byte.
    // For example:
    //  0     -> isSigned = false, bytes = {0}
    //  255   -> isSigned = false, bytes = {0xFF}
    //  -255  -> isSigned = true,  bytes = {0xFF}
    struct NumberCmdValue final
    {
        bool isSigned = false;
        std::vector<std::uint8_t> bytes;
    };

    struct BoolCmdValue final
    {
        bool value = false;
    };

    using PrimitiveCmdValue = std::variant<
        UuidCmdValue,
        CharSeqCmdValue,
        NumberCmdValue,
        BoolCmdValue
    >;

    using KeyCmdValue = PrimitiveCmdValue;

    struct PlainCmdValue final
    {
        PrimitiveCmdValue value;
    };

    struct NullableCmdValue final
    {
        std::optional<PrimitiveCmdValue> value;
    };

    struct ArrayCmdValue final
    {
        std::vector<PrimitiveCmdValue> items;
    };

    struct ArrayOfNullableCmdValue final
    {
        std::vector<std::optional<PrimitiveCmdValue>> items;
    };

    struct NullableArrayCmdValue final
    {
        std::optional<std::vector<PrimitiveCmdValue>> items;
    };

    struct NullableArrayOfNullableCmdValue final
    {
        std::optional<std::vector<std::optional<PrimitiveCmdValue>>> items;
    };

    using ColCmdValue = std::variant<
        PlainCmdValue,
        NullableCmdValue,
        ArrayCmdValue,
        ArrayOfNullableCmdValue,
        NullableArrayCmdValue,
        NullableArrayOfNullableCmdValue
    >;
}
