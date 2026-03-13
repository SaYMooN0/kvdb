#pragma once

#include <cstdint>
#include <string>
#include <utility>

#include "kvdb_exception.h"

namespace kvdb::contracts {
    class ColType final
    {
    public:
        enum class PrimitiveKind : std::uint8_t
        {
            Uuid = 0,
            CharSeq = 1,
            Int = 2,
            UInt = 3,
            Bool = 4
        };

        // All allowed wrapper type forms.
        enum class WrapperForm : std::uint8_t
        {
            Plain = 0,                     // T
            Nullable = 1,                 // nullable(T)
            Array = 2,                    // array(T)
            ArrayOfNullable = 3,          // array(nullable(T))
            NullableArray = 4,            // nullable(array(T))
            NullableArrayOfNullable = 5   // nullable(array(nullable(T)))
        };

    public:
        [[nodiscard]] static ColType Uuid() {
            return Make(PrimitiveKind::Uuid, WrapperForm::Plain, 0);
        }

        [[nodiscard]] static ColType Bool() {
            return Make(PrimitiveKind::Bool, WrapperForm::Plain, 0);
        }

        [[nodiscard]] static ColType CharSeq(std::uint32_t maxLength) {
            if (maxLength < 1 || maxLength > 65535) {
                throw KvdbException::invalidCharSeqLength(
                    "Requested max length: " + std::to_string(maxLength) + "."
                );
            }

            return Make(
                PrimitiveKind::CharSeq,
                WrapperForm::Plain,
                static_cast<std::uint16_t>(maxLength)
            );
        }

        [[nodiscard]] static ColType Int(std::uint32_t byteCount) {
            if (byteCount < 1 || byteCount > 8) {
                throw KvdbException::invalidIntByteCount(
                    "Requested byte count: " + std::to_string(byteCount) + "."
                );
            }

            return Make(
                PrimitiveKind::Int,
                WrapperForm::Plain,
                static_cast<std::uint16_t>(byteCount)
            );
        }

        [[nodiscard]] static ColType UInt(std::uint32_t byteCount) {
            if (byteCount < 1 || byteCount > 8) {
                throw KvdbException::invalidUIntByteCount(
                    "Requested byte count: " + std::to_string(byteCount) + "."
                );
            }

            return Make(
                PrimitiveKind::UInt,
                WrapperForm::Plain,
                static_cast<std::uint16_t>(byteCount)
            );
        }

        [[nodiscard]] ColType AsNullable() const {
            switch (form()) {
                case WrapperForm::Plain:
                    return Make(primitive(), WrapperForm::Nullable, parameter());

                case WrapperForm::Array:
                    return Make(primitive(), WrapperForm::NullableArray, parameter());

                case WrapperForm::ArrayOfNullable:
                    return Make(primitive(), WrapperForm::NullableArrayOfNullable, parameter());

                case WrapperForm::Nullable:
                case WrapperForm::NullableArray:
                case WrapperForm::NullableArrayOfNullable:
                    throw KvdbException::nestedNullableType(
                        "Attempted type composition: nullable(" + ToString() + ")."
                    );
            }

            throw KvdbException::programBug(
                "Unsupported wrapper form in ColType::AsNullable()."
            );
        }

        [[nodiscard]] ColType AsArray() const {
            switch (form()) {
                case WrapperForm::Plain:
                    return Make(primitive(), WrapperForm::Array, parameter());

                case WrapperForm::Nullable:
                    return Make(primitive(), WrapperForm::ArrayOfNullable, parameter());

                case WrapperForm::Array:
                case WrapperForm::ArrayOfNullable:
                case WrapperForm::NullableArray:
                case WrapperForm::NullableArrayOfNullable:
                    throw KvdbException::nestedArrayType(
                        "Attempted type composition: array(" + ToString() + ")."
                    );
            }

            throw KvdbException::programBug(
                "Unsupported wrapper form in ColType::AsArray()."
            );
        }

        [[nodiscard]] PrimitiveKind primitive() const noexcept {
            return static_cast<PrimitiveKind>(raw_ & PrimitiveMask);
        }

        [[nodiscard]] WrapperForm form() const noexcept {
            return static_cast<WrapperForm>((raw_ >> FormShift) & FormMaskValue);
        }

        [[nodiscard]] std::uint16_t parameter() const noexcept {
            return static_cast<std::uint16_t>((raw_ >> ParamShift) & ParamMaskValue);
        }

        [[nodiscard]] bool hasArray() const noexcept {
            switch (form()) {
                case WrapperForm::Array:
                case WrapperForm::ArrayOfNullable:
                case WrapperForm::NullableArray:
                case WrapperForm::NullableArrayOfNullable:
                    return true;

                case WrapperForm::Plain:
                case WrapperForm::Nullable:
                    return false;
            }

            return false;
        }

        [[nodiscard]] bool hasOuterNullable() const noexcept {
            switch (form()) {
                case WrapperForm::Nullable:
                case WrapperForm::NullableArray:
                case WrapperForm::NullableArrayOfNullable:
                    return true;

                case WrapperForm::Plain:
                case WrapperForm::Array:
                case WrapperForm::ArrayOfNullable:
                    return false;
            }

            return false;
        }

        [[nodiscard]] bool hasInnerNullable() const noexcept {
            switch (form()) {
                case WrapperForm::ArrayOfNullable:
                case WrapperForm::NullableArrayOfNullable:
                    return true;

                case WrapperForm::Plain:
                case WrapperForm::Nullable:
                case WrapperForm::Array:
                case WrapperForm::NullableArray:
                    return false;
            }

            return false;
        }

        [[nodiscard]] bool hasAnyNullable() const noexcept {
            return hasOuterNullable() || hasInnerNullable();
        }

        [[nodiscard]] bool isAllowedAsKey() const noexcept {
            return !hasArray() && !hasAnyNullable();
        }

        void throwIfNotAllowedAsKey() const {
            if (hasArray()) {
                throw KvdbException::arrayKeyType(
                    "Attempted key type: " + ToString() + "."
                );
            }

            if (hasAnyNullable()) {
                throw KvdbException::nullableKeyType(
                    "Attempted key type: " + ToString() + "."
                );
            }
        }

        [[nodiscard]] bool isCharSeq() const noexcept {
            return primitive() == PrimitiveKind::CharSeq;
        }

        [[nodiscard]] bool isInt() const noexcept {
            return primitive() == PrimitiveKind::Int;
        }

        [[nodiscard]] bool isUInt() const noexcept {
            return primitive() == PrimitiveKind::UInt;
        }

        [[nodiscard]] bool isBool() const noexcept {
            return primitive() == PrimitiveKind::Bool;
        }

        [[nodiscard]] bool isUuid() const noexcept {
            return primitive() == PrimitiveKind::Uuid;
        }

        [[nodiscard]] bool isFixedSize() const noexcept {
            if (hasArray()) {
                return false;
            }

            if (primitive() == PrimitiveKind::CharSeq) {
                return false;
            }

            return true;
        }

        [[nodiscard]] std::uint32_t fixedSizeInBytes() const {
            if (!isFixedSize()) {
                throw KvdbException::programBug(
                    "Fixed size was requested for a variable-size type.",
                    "Type: " + ToString()
                );
            }

            std::uint32_t size = primitiveFixedSizeInBytes();

            if (form() == WrapperForm::Nullable) {
                size += 1; // null flag
            }

            return size;
        }

        [[nodiscard]] std::uint16_t charSeqMaxLength() const {
            if (primitive() != PrimitiveKind::CharSeq) {
                throw KvdbException::programBug(
                    "charSeqMaxLength() was called for a non-charseq type.",
                    "Type: " + ToString()
                );
            }

            return parameter();
        }

        [[nodiscard]] std::uint8_t integerByteCount() const {
            if (primitive() != PrimitiveKind::Int && primitive() != PrimitiveKind::UInt) {
                throw KvdbException::programBug(
                    "integerByteCount() was called for a non-integer type.",
                    "Type: " + ToString()
                );
            }

            return static_cast<std::uint8_t>(parameter());
        }

        [[nodiscard]] std::uint32_t raw() const noexcept {
            return raw_;
        }

        [[nodiscard]] std::string ToString() const {
            const std::string leaf = leafToString();

            switch (form()) {
                case WrapperForm::Plain:
                    return leaf;

                case WrapperForm::Nullable:
                    return "nullable(" + leaf + ")";

                case WrapperForm::Array:
                    return "array(" + leaf + ")";

                case WrapperForm::ArrayOfNullable:
                    return "array(nullable(" + leaf + "))";

                case WrapperForm::NullableArray:
                    return "nullable(array(" + leaf + "))";

                case WrapperForm::NullableArrayOfNullable:
                    return "nullable(array(nullable(" + leaf + ")))";
            }

            throw KvdbException::programBug(
                "Unsupported wrapper form in ColType::ToString()."
            );
        }

    private:
        static constexpr std::uint32_t PrimitiveMask = 0b111u;
        static constexpr std::uint32_t FormShift = 3u;
        static constexpr std::uint32_t FormMaskValue = 0b111u;
        static constexpr std::uint32_t ParamShift = 6u;
        static constexpr std::uint32_t ParamMaskValue = 0xFFFFu;

    private:
        explicit constexpr ColType(std::uint32_t raw) noexcept
            : raw_(raw) {
        }

        [[nodiscard]] static ColType Make(
            PrimitiveKind primitiveKind,
            WrapperForm wrapperForm,
            std::uint16_t parameter
        ) {
            const auto primitiveBits = static_cast<std::uint32_t>(primitiveKind);
            const auto formBits = static_cast<std::uint32_t>(wrapperForm);

            const std::uint32_t raw =
                primitiveBits |
                (formBits << FormShift) |
                (static_cast<std::uint32_t>(parameter) << ParamShift);

            return ColType(raw);
        }

        [[nodiscard]] std::string leafToString() const {
            switch (primitive()) {
                case PrimitiveKind::Uuid:
                    return "uuid";

                case PrimitiveKind::Bool:
                    return "bool";

                case PrimitiveKind::CharSeq:
                    return "charseq(" + std::to_string(parameter()) + ")";

                case PrimitiveKind::Int:
                    return "int(" + std::to_string(parameter()) + ")";

                case PrimitiveKind::UInt:
                    return "uint(" + std::to_string(parameter()) + ")";
            }

            throw KvdbException::programBug(
                "Unsupported primitive kind in ColType::leafToString()."
            );
        }

        [[nodiscard]] std::uint32_t primitiveFixedSizeInBytes() const {
            switch (primitive()) {
                case PrimitiveKind::Uuid:
                    return 16;

                case PrimitiveKind::Bool:
                    return 1;

                case PrimitiveKind::Int:
                case PrimitiveKind::UInt:
                    return parameter();

                case PrimitiveKind::CharSeq:
                    throw KvdbException::programBug(
                        "primitiveFixedSizeInBytes() was called for charseq.",
                        "Type: " + ToString()
                    );
            }

            throw KvdbException::programBug(
                "Unsupported primitive kind in ColType::primitiveFixedSizeInBytes()."
            );
        }

    private:
        std::uint32_t raw_ = 0;
    };
}