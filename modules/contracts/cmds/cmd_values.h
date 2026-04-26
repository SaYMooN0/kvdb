#pragma once

#include <array>
#include <cstdint>
#include <cstddef>

namespace kvdb::contracts {
    struct UuidCmdValue final
    {
        std::array<std::uint8_t, 16> bytes{};
    };

    struct CharSeqCmdValue final
    {
        const char* utf8Value = nullptr;
        std::uint32_t byteLength = 0;
    };

    struct NumberCmdValue final
    {
        bool isSigned = false;
        std::array<std::uint8_t, 16> bytes{};
        // for int(4), int(8), int(16)
        std::uint8_t byteLength = 0;
    };

    struct BoolCmdValue final
    {
        bool value = false;
    };

    enum class PrimitiveCmdValueKind : std::uint8_t
    {
        Uuid,
        CharSeq,
        Number,
        Bool
    };

    struct PrimitiveCmdValue final
    {
        PrimitiveCmdValueKind kind;

        union
        {
            UuidCmdValue uuid;
            CharSeqCmdValue charSeq;
            NumberCmdValue number;
            BoolCmdValue boolean;
        };

        PrimitiveCmdValue() : kind(PrimitiveCmdValueKind::Bool), boolean{} {}
    };

    using KeyCmdValue = PrimitiveCmdValue;

    enum class ColCmdValueKind : std::uint8_t
    {
        Plain,
        Nullable,
        Array,
        ArrayOfNullable,
        NullableArray,
        NullableArrayOfNullable
    };

    struct NullablePrimitiveCmdValue final
    {
        bool hasValue = false;
        PrimitiveCmdValue value;
    };

    struct PrimitiveCmdValueArrayView final
    {
        const PrimitiveCmdValue* items = nullptr;
        std::uint32_t count = 0;
    };

    struct NullablePrimitiveCmdValueArrayView final
    {
        const NullablePrimitiveCmdValue* items = nullptr;
        std::uint32_t count = 0;
    };

    struct ColCmdValue final
    {
        ColCmdValueKind kind;

        union
        {
            PrimitiveCmdValue plain;
            NullablePrimitiveCmdValue nullable;

            PrimitiveCmdValueArrayView array;
            NullablePrimitiveCmdValueArrayView arrayOfNullable;

            // nullable array:
            // hasValue=false means null
            struct
            {
                bool hasValue;
                PrimitiveCmdValueArrayView value;
            } nullableArray;

            struct
            {
                bool hasValue;
                NullablePrimitiveCmdValueArrayView value;
            } nullableArrayOfNullable;
        };

        ColCmdValue() : kind(ColCmdValueKind::Plain), plain{} {}
    };

    enum class CmdTypeKind : std::uint8_t
    {
        Uuid,
        CharSeq,
        Int,
        Bool,
        Nullable,
        Array
    };

    struct CmdTypeKindValue final
    {
        CmdTypeKind type;

        // для int(4), charseq(4)
        std::uint16_t sizeParam = 0;

        // для nullable(...), array(...)
        const CmdTypeKindValue* typeParam = nullptr;
    };
}
