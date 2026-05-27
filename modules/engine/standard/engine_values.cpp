#include "engine_common.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <utility>

namespace kvdb::modules::engine::standard {
    namespace {
        struct EngineErr final : Err
        {
            std::string codeValue;
            std::string messageValue;

            EngineErr(std::string code, std::string message)
                : codeValue(std::move(code)),
                  messageValue(std::move(message)) {}

            [[nodiscard]]
            std::string_view module() const noexcept override {
                return "engine";
            }

            [[nodiscard]]
            std::string_view code() const noexcept override {
                return codeValue;
            }

            [[nodiscard]]
            std::string_view message() const noexcept override {
                return messageValue;
            }
        };
    }

    CmdExecErr makeErr(std::string code, std::string message) {
        return std::make_shared<EngineErr>(std::move(code), std::move(message));
    }

    CmdExecResult makeEmptySuccess() {
        return SuccessCmdExecResult{EmptyCmdExecSuccess{}};
    }

    CmdExecResult makeTransactionSuccess(TransactionOpKind operation) {
        return SuccessCmdExecResult{
            TransactionOpCmdExecSuccess{
                .operation = operation
            }
        };
    }

    CmdExecResult makeAffectedRowsSuccess(std::uint64_t count) {
        return SuccessCmdExecResult{
            AffectedRowsCmdExecSuccess{
                .count = count
            }
        };
    }

    DbType copyType(const CmdTypeKindValue& value) {
        DbType result;
        result.type = value.type;
        result.sizeParam = value.sizeParam;

        if (value.typeParam != nullptr) {
            result.typeParam = std::make_shared<DbType>(copyType(*value.typeParam));
        }

        return result;
    }

    bool isPrimitiveType(const DbType& type) {
        return type.type == CmdTypeKind::Uuid
            || type.type == CmdTypeKind::CharSeq
            || type.type == CmdTypeKind::Int
            || type.type == CmdTypeKind::UInt
            || type.type == CmdTypeKind::Bool
            || type.type == CmdTypeKind::Float;
    }

    std::uint64_t doubleToBits(double value) {
        std::uint64_t result = 0;
        static_assert(sizeof(result) == sizeof(value));
        std::memcpy(&result, &value, sizeof(value));
        return result;
    }

    bool operator==(const StoredNumber& left, const StoredNumber& right) {
        return left.isSigned == right.isSigned
            && left.byteLength == right.byteLength
            && left.bytes == right.bytes;
    }

    bool operator==(const StoredPrimitive& left, const StoredPrimitive& right) {
        if (left.kind != right.kind) {
            return false;
        }

        switch (left.kind) {
        case StoredPrimitiveKind::Uuid:
            return left.uuid == right.uuid;

        case StoredPrimitiveKind::CharSeq:
            return left.charSeq == right.charSeq;

        case StoredPrimitiveKind::Number:
            return left.number == right.number;

        case StoredPrimitiveKind::Bool:
            return left.boolean == right.boolean;

        case StoredPrimitiveKind::Float:
            return doubleToBits(left.floating) == doubleToBits(right.floating);
        }

        return false;
    }

    void hashCombine(std::size_t& seed, std::size_t value) {
        seed ^= value + 0x9e3779b97f4a7c15ULL + (seed << 6U) + (seed >> 2U);
    }

    std::size_t StoredPrimitiveHash::operator()(const StoredPrimitive& value) const {
        std::size_t seed = std::hash<int>{}(static_cast<int>(value.kind));

        switch (value.kind) {
        case StoredPrimitiveKind::Uuid:
            for (const auto byte : value.uuid) {
                hashCombine(seed, std::hash<std::uint8_t>{}(byte));
            }
            break;

        case StoredPrimitiveKind::CharSeq:
            hashCombine(seed, std::hash<std::string>{}(value.charSeq));
            break;

        case StoredPrimitiveKind::Number:
            hashCombine(seed, std::hash<bool>{}(value.number.isSigned));
            hashCombine(seed, std::hash<std::uint8_t>{}(value.number.byteLength));
            for (std::uint8_t i = 0; i < value.number.byteLength; ++i) {
                hashCombine(seed, std::hash<std::uint8_t>{}(value.number.bytes[i]));
            }
            break;

        case StoredPrimitiveKind::Bool:
            hashCombine(seed, std::hash<bool>{}(value.boolean));
            break;

        case StoredPrimitiveKind::Float:
            hashCombine(seed, std::hash<std::uint64_t>{}(doubleToBits(value.floating)));
            break;
        }

        return seed;
    }

    void ResultDtoStorage::clear() {
        strings.clear();
        primitiveArrays.clear();
        nullablePrimitiveArrays.clear();
        typeNodes.clear();
    }

    CmdTypeKindValue toDtoType(const DbType& type, ResultDtoStorage& storage) {
        CmdTypeKindValue result{
            .type = type.type,
            .sizeParam = type.sizeParam,
            .typeParam = nullptr
        };

        if (type.typeParam != nullptr) {
            auto inner = std::make_unique<CmdTypeKindValue>(
                toDtoType(*type.typeParam, storage)
            );

            result.typeParam = inner.get();
            storage.typeNodes.push_back(std::move(inner));
        }

        return result;
    }

    ColCmdValueKind colValueKind(const StoredColValue& value) {
        return std::visit(
            [](const auto& concreteValue) -> ColCmdValueKind {
                using ValueType = std::decay_t<decltype(concreteValue)>;

                if constexpr (std::is_same_v<ValueType, StoredPlainColValue>) {
                    return ColCmdValueKind::Plain;
                }
                else if constexpr (std::is_same_v<ValueType, StoredNullableColValue>) {
                    return ColCmdValueKind::Nullable;
                }
                else if constexpr (std::is_same_v<ValueType, StoredArrayColValue>) {
                    return ColCmdValueKind::Array;
                }
                else if constexpr (std::is_same_v<ValueType, StoredArrayOfNullableColValue>) {
                    return ColCmdValueKind::ArrayOfNullable;
                }
                else if constexpr (std::is_same_v<ValueType, StoredNullableArrayColValue>) {
                    return ColCmdValueKind::NullableArray;
                }
                else {
                    return ColCmdValueKind::NullableArrayOfNullable;
                }
            },
            value
        );
    }

    bool isValidNumberByteLength(const std::uint8_t byteLength) {
        return byteLength >= 1 && byteLength <= MaxNumberByteLength;
    }

    bool hasOnlyByteValueFrom(
        const std::array<std::uint8_t, 16>& bytes,
        const std::size_t from,
        const std::size_t to,
        const std::uint8_t expectedByte
    ) {
        for (std::size_t i = from; i < to; ++i) {
            if (bytes[i] != expectedByte) {
                return false;
            }
        }

        return true;
    }

    bool isNegativeSignedNumber(const NumberCmdValue& value) {
        return value.isSigned
            && isValidNumberByteLength(value.byteLength)
            && (value.bytes[value.byteLength - 1] & 0x80U) != 0;
    }

    bool signedNumberFitsIntoSignedBytes(
        const NumberCmdValue& value,
        const std::uint8_t targetByteLength
    ) {
        const auto sourceByteLength = static_cast<std::size_t>(value.byteLength);
        const auto targetSize = static_cast<std::size_t>(targetByteLength);

        if (sourceByteLength <= targetSize) {
            return true;
        }

        const bool targetNegative = (value.bytes[targetSize - 1] & 0x80U) != 0;
        const std::uint8_t expectedExtensionByte = targetNegative ? 0xFFU : 0x00U;

        return hasOnlyByteValueFrom(
            value.bytes,
            targetSize,
            sourceByteLength,
            expectedExtensionByte
        );
    }

    bool unsignedNumberFitsIntoUnsignedBytes(
        const NumberCmdValue& value,
        const std::uint8_t targetByteLength
    ) {
        const auto sourceByteLength = static_cast<std::size_t>(value.byteLength);
        const auto targetSize = static_cast<std::size_t>(targetByteLength);

        if (sourceByteLength <= targetSize) {
            return true;
        }

        return hasOnlyByteValueFrom(value.bytes, targetSize, sourceByteLength, 0x00U);
    }

    bool unsignedNumberFitsIntoSignedBytes(
        const NumberCmdValue& value,
        const std::uint8_t targetByteLength
    ) {
        if (!unsignedNumberFitsIntoUnsignedBytes(value, targetByteLength)) {
            return false;
        }

        const auto sourceByteLength = static_cast<std::size_t>(value.byteLength);
        const auto checkedByteIndex = static_cast<std::size_t>(targetByteLength) - 1;

        if (checkedByteIndex >= sourceByteLength) {
            return true;
        }

        return (value.bytes[checkedByteIndex] & 0x80U) == 0;
    }

    StoredNumber makeStoredNumber(
        const NumberCmdValue& value,
        const bool isSigned,
        const std::uint8_t targetByteLength,
        const std::uint8_t fillByte
    ) {
        StoredNumber result;
        result.isSigned = isSigned;
        result.byteLength = targetByteLength;
        result.bytes.assign(static_cast<std::size_t>(targetByteLength), fillByte);

        const auto bytesToCopy = std::min<std::size_t>(
            value.byteLength,
            targetByteLength
        );

        std::copy_n(value.bytes.begin(), bytesToCopy, result.bytes.begin());
        return result;
    }

    std::string formatByteLimit(const std::uint16_t declaredByteLength) {
        return std::to_string(declaredByteLength) + " byte(s) (" +
            std::to_string(static_cast<std::uint32_t>(declaredByteLength) * 8U) + " bits)";
    }

    std::string formatActualByteLength(const std::uint8_t byteLength) {
        return std::to_string(static_cast<std::uint32_t>(byteLength)) + " byte(s)";
    }

    std::variant<StoredNumber, CmdExecErr> normalizeSignedInteger(
        const NumberCmdValue& value,
        const std::uint16_t declaredByteLength
    ) {
        if (!isValidNumberByteLength(value.byteLength)) {
            return invalidValue(
                "Invalid number byte length: " + formatActualByteLength(value.byteLength) + ". Allowed range is 1..16 byte(s).");
        }

        if (declaredByteLength == 0 || declaredByteLength > MaxNumberByteLength) {
            return schemaMismatch(
                "Declared int byte count must be between 1 and 16. Actual: " +
                std::to_string(declaredByteLength) + "."
            );
        }

        const auto targetByteLength = static_cast<std::uint8_t>(declaredByteLength);

        if (value.isSigned) {
            if (!signedNumberFitsIntoSignedBytes(value, targetByteLength)) {
                return invalidValue(
                    "Number does not fit into declared int byte count. Limit: " +
                    formatByteLimit(declaredByteLength) + "."
                );
            }

            return makeStoredNumber(
                value,
                true,
                targetByteLength,
                isNegativeSignedNumber(value) ? 0xFFU : 0x00U
            );
        }

        if (!unsignedNumberFitsIntoSignedBytes(value, targetByteLength)) {
            return invalidValue(
                "Unsigned number does not fit into declared int byte count. Limit: " +
                formatByteLimit(declaredByteLength) + "."
            );
        }

        return makeStoredNumber(value, true, targetByteLength, 0x00U);
    }

    std::variant<StoredNumber, CmdExecErr> normalizeUnsignedInteger(
        const NumberCmdValue& value,
        const std::uint16_t declaredByteLength
    ) {
        if (!isValidNumberByteLength(value.byteLength)) {
            return invalidValue(
                "Invalid number byte length: " + formatActualByteLength(value.byteLength) + ". Allowed range is 1..16 byte(s).");
        }

        if (declaredByteLength == 0 || declaredByteLength > MaxNumberByteLength) {
            return schemaMismatch(
                "Declared uint byte count must be between 1 and 16. Actual: " +
                std::to_string(declaredByteLength) + "."
            );
        }

        const auto targetByteLength = static_cast<std::uint8_t>(declaredByteLength);

        if (value.isSigned) {
            if (isNegativeSignedNumber(value)) {
                return invalidValue("Negative number cannot be used as uint.");
            }

            if (!unsignedNumberFitsIntoUnsignedBytes(value, targetByteLength)) {
                return invalidValue(
                    "Number does not fit into declared uint byte count. Limit: " +
                    formatByteLimit(declaredByteLength) + "."
                );
            }

            return makeStoredNumber(value, false, targetByteLength, 0x00U);
        }

        if (!unsignedNumberFitsIntoUnsignedBytes(value, targetByteLength)) {
            return invalidValue(
                "Number does not fit into declared uint byte count. Limit: " +
                formatByteLimit(declaredByteLength) + "."
            );
        }

        return makeStoredNumber(value, false, targetByteLength, 0x00U);
    }

    double readUnsignedNumberAsDouble(const NumberCmdValue& value) {
        if (!isValidNumberByteLength(value.byteLength)) {
            throw std::runtime_error("Invalid number byte length.");
        }

        double result = 0.0;

        for (std::size_t i = value.byteLength; i > 0; --i) {
            result = result * 256.0 + static_cast<double>(value.bytes[i - 1]);
        }

        return result;
    }

    double readSignedNumberAsDouble(const NumberCmdValue& value) {
        if (!isValidNumberByteLength(value.byteLength)) {
            throw std::runtime_error("Invalid signed number byte length.");
        }

        if (!isNegativeSignedNumber(value)) {
            return readUnsignedNumberAsDouble(value);
        }

        std::array<std::uint8_t, 16> magnitudeBytes = value.bytes;

        for (std::size_t i = 0; i < value.byteLength; ++i) {
            magnitudeBytes[i] = static_cast<std::uint8_t>(~magnitudeBytes[i]);
        }

        std::uint16_t carry = 1;
        for (std::size_t i = 0; i < value.byteLength; ++i) {
            const std::uint16_t sum = static_cast<std::uint16_t>(magnitudeBytes[i]) + carry;
            magnitudeBytes[i] = static_cast<std::uint8_t>(sum & 0xFFU);
            carry = static_cast<std::uint16_t>(sum >> 8U);
        }

        NumberCmdValue magnitude;
        magnitude.isSigned = false;
        magnitude.bytes = magnitudeBytes;
        magnitude.byteLength = value.byteLength;

        return -readUnsignedNumberAsDouble(magnitude);
    }

    CmdExecErr schemaMismatch(std::string message) {
        return makeErr("SchemaMismatch", std::move(message));
    }

    CmdExecErr invalidValue(std::string message) {
        return makeErr("InvalidValue", std::move(message));
    }

    std::optional<CmdExecErr> validateDbType(
        const DbType& type,
        const bool isKeyType
    ) {
        if (isKeyType && type.type == CmdTypeKind::Nullable) {
            return makeErr("InvalidKeyType", "Key type cannot be nullable.");
        }

        if (isKeyType && type.type == CmdTypeKind::Array) {
            return makeErr("InvalidKeyType", "Key type cannot be an array.");
        }

        switch (type.type) {
        case CmdTypeKind::Uuid:
        case CmdTypeKind::Bool:
        case CmdTypeKind::Float:
            return std::nullopt;

        case CmdTypeKind::CharSeq:
            if (type.sizeParam == 0) {
                return makeErr(
                    "InvalidType",
                    "Declared charseq length must be greater than zero."
                );
            }

            return std::nullopt;

        case CmdTypeKind::Int:
            if (type.sizeParam == 0 || type.sizeParam > MaxNumberByteLength) {
                return makeErr(
                    "InvalidType",
                    "Declared int byte count must be between 1 and 16."
                );
            }

            return std::nullopt;

        case CmdTypeKind::UInt:
            if (type.sizeParam == 0 || type.sizeParam > MaxNumberByteLength) {
                return makeErr(
                    "InvalidType",
                    "Declared uint byte count must be between 1 and 16."
                );
            }

            return std::nullopt;

        case CmdTypeKind::Nullable:
            if (!type.typeParam) {
                return makeErr("InvalidType", "Nullable type does not have inner type.");
            }

            if (type.typeParam->type == CmdTypeKind::Nullable) {
                return makeErr("InvalidType", "Nested nullable types are not allowed.");
            }

            return validateDbType(*type.typeParam, false);

        case CmdTypeKind::Array:
            if (!type.typeParam) {
                return makeErr("InvalidType", "Array type does not have inner type.");
            }

            if (type.typeParam->type == CmdTypeKind::Array) {
                return makeErr("InvalidType", "Nested array types are not allowed.");
            }

            return validateDbType(*type.typeParam, false);
        }

        return makeErr("InvalidType", "Unsupported type.");
    }

    std::optional<CmdExecErr> validateTableSchema(const Table& table) {
        if (const auto keyErr = validateDbType(table.keyType, true)) {
            return keyErr;
        }

        return validateDbType(table.valueType, false);
    }

    std::variant<StoredPrimitive, CmdExecErr> normalizePrimitive(
        const PrimitiveCmdValue& value,
        const DbType& expectedType
    ) {
        if (!isPrimitiveType(expectedType)) {
            return schemaMismatch("Expected primitive type.");
        }

        switch (expectedType.type) {
        case CmdTypeKind::Uuid: {
            if (value.kind != PrimitiveCmdValueKind::Uuid) {
                return schemaMismatch("Expected uuid value.");
            }

            StoredPrimitive result;
            result.kind = StoredPrimitiveKind::Uuid;
            result.uuid = value.uuid.bytes;
            return result;
        }

        case CmdTypeKind::CharSeq: {
            if (value.kind != PrimitiveCmdValueKind::CharSeq) {
                return schemaMismatch("Expected charseq value.");
            }

            const auto size = static_cast<std::size_t>(value.charSeq.byteLength);

            if (value.charSeq.utf8Value == nullptr && size != 0) {
                return invalidValue("Charseq value points to null.");
            }

            if (expectedType.sizeParam != 0 && size > expectedType.sizeParam) {
                return schemaMismatch(
                    "Charseq value is longer than the declared charseq size. Limit: " + std::to_string(expectedType.sizeParam) +
                    " byte(s). Actual: " + std::to_string(size) + " byte(s).");
            }

            StoredPrimitive result;
            result.kind = StoredPrimitiveKind::CharSeq;
            result.charSeq = std::string(value.charSeq.utf8Value, size);
            return result;
        }

        case CmdTypeKind::Int: {
            if (value.kind != PrimitiveCmdValueKind::Number) {
                return schemaMismatch("Expected int value.");
            }

            auto normalized = normalizeSignedInteger(
                value.number,
                expectedType.sizeParam
            );

            if (std::holds_alternative<CmdExecErr>(normalized)) {
                return std::get<CmdExecErr>(std::move(normalized));
            }

            StoredPrimitive result;
            result.kind = StoredPrimitiveKind::Number;
            result.number = std::get<StoredNumber>(std::move(normalized));
            return result;
        }

        case CmdTypeKind::UInt: {
            if (value.kind != PrimitiveCmdValueKind::Number) {
                return schemaMismatch("Expected uint value.");
            }

            auto normalized = normalizeUnsignedInteger(
                value.number,
                expectedType.sizeParam
            );

            if (std::holds_alternative<CmdExecErr>(normalized)) {
                return std::get<CmdExecErr>(std::move(normalized));
            }

            StoredPrimitive result;
            result.kind = StoredPrimitiveKind::Number;
            result.number = std::get<StoredNumber>(std::move(normalized));
            return result;
        }

        case CmdTypeKind::Bool: {
            if (value.kind != PrimitiveCmdValueKind::Bool) {
                return schemaMismatch("Expected bool value.");
            }

            StoredPrimitive result;
            result.kind = StoredPrimitiveKind::Bool;
            result.boolean = value.boolean.value;
            return result;
        }

        case CmdTypeKind::Float: {
            StoredPrimitive result;
            result.kind = StoredPrimitiveKind::Float;

            if (value.kind == PrimitiveCmdValueKind::Float) {
                result.floating = value.floating.value;
                return result;
            }

            if (value.kind == PrimitiveCmdValueKind::Number) {
                try {
                    if (value.number.isSigned) {
                        result.floating = readSignedNumberAsDouble(value.number);
                    }
                    else {
                        result.floating = readUnsignedNumberAsDouble(value.number);
                    }
                }
                catch (const std::exception& ex) {
                    return invalidValue(ex.what());
                }

                return result;
            }

            return schemaMismatch("Expected float value.");
        }

        case CmdTypeKind::Nullable:
        case CmdTypeKind::Array:
            return schemaMismatch("Expected primitive type.");
        }

        return schemaMismatch("Unsupported primitive type.");
    }

    bool isErr(const std::variant<StoredPrimitive, CmdExecErr>& value) {
        return std::holds_alternative<CmdExecErr>(value);
    }

    StoredPrimitive takePrimitive(std::variant<StoredPrimitive, CmdExecErr>&& value) {
        return std::get<StoredPrimitive>(std::move(value));
    }

    CmdExecErr takeErr(std::variant<StoredPrimitive, CmdExecErr>&& value) {
        return std::get<CmdExecErr>(std::move(value));
    }

    std::variant<StoredColValue, CmdExecErr> normalizeColValue(
        const ColCmdValue& value,
        const DbType& expectedType
    ) {
        if (isPrimitiveType(expectedType)) {
            if (value.kind != ColCmdValueKind::Plain) {
                return schemaMismatch("Expected plain primitive value.");
            }

            auto normalized = normalizePrimitive(value.plain, expectedType);
            if (isErr(normalized)) {
                return takeErr(std::move(normalized));
            }

            return StoredPlainColValue{
                .value = takePrimitive(std::move(normalized))
            };
        }

        if (expectedType.type == CmdTypeKind::Nullable) {
            if (!expectedType.typeParam) {
                return schemaMismatch("Nullable type does not have inner type.");
            }

            const auto& innerType = *expectedType.typeParam;

            if (value.kind == ColCmdValueKind::Nullable && !value.nullable.hasValue) {
                if (innerType.type == CmdTypeKind::Array) {
                    if (!innerType.typeParam) {
                        return schemaMismatch("Array type does not have inner type.");
                    }

                    if (innerType.typeParam->type == CmdTypeKind::Nullable) {
                        return StoredNullableArrayOfNullableColValue{.items = std::nullopt};
                    }

                    return StoredNullableArrayColValue{.items = std::nullopt};
                }

                return StoredNullableColValue{.value = std::nullopt};
            }

            if (isPrimitiveType(innerType)) {
                PrimitiveCmdValue primitiveValue{};

                if (value.kind == ColCmdValueKind::Plain) {
                    primitiveValue = value.plain;
                }
                else if (value.kind == ColCmdValueKind::Nullable && value.nullable.hasValue) {
                    primitiveValue = value.nullable.value;
                }
                else {
                    return schemaMismatch("Expected nullable primitive value.");
                }

                auto normalized = normalizePrimitive(primitiveValue, innerType);
                if (isErr(normalized)) {
                    return takeErr(std::move(normalized));
                }

                return StoredNullableColValue{
                    .value = takePrimitive(std::move(normalized))
                };
            }

            if (innerType.type == CmdTypeKind::Array) {
                auto normalizedArray = normalizeColValue(value, innerType);
                if (std::holds_alternative<CmdExecErr>(normalizedArray)) {
                    return std::get<CmdExecErr>(std::move(normalizedArray));
                }

                auto arrayValue = std::get<StoredColValue>(std::move(normalizedArray));

                if (auto* concreteArray = std::get_if<StoredArrayColValue>(&arrayValue)) {
                    return StoredNullableArrayColValue{
                        .items = std::move(concreteArray->items)
                    };
                }

                if (auto* concreteArray = std::get_if<StoredArrayOfNullableColValue>(&arrayValue)) {
                    return StoredNullableArrayOfNullableColValue{
                        .items = std::move(concreteArray->items)
                    };
                }

                return schemaMismatch("Expected array value inside nullable.");
            }

            return schemaMismatch("Unsupported nullable inner type.");
        }

        if (expectedType.type == CmdTypeKind::Array) {
            if (!expectedType.typeParam) {
                return schemaMismatch("Array type does not have inner type.");
            }

            const auto& innerType = *expectedType.typeParam;

            if (innerType.type == CmdTypeKind::Nullable) {
                if (!innerType.typeParam) {
                    return schemaMismatch("Nullable array item type does not have inner type.");
                }

                if (value.kind != ColCmdValueKind::Array
                    && value.kind != ColCmdValueKind::ArrayOfNullable) {
                    return schemaMismatch("Expected array value.");
                }

                StoredArrayOfNullableColValue result;

                if (value.kind == ColCmdValueKind::Array) {
                    result.items.reserve(value.array.count);

                    for (std::uint32_t i = 0; i < value.array.count; ++i) {
                        auto normalized = normalizePrimitive(
                            value.array.items[i],
                            *innerType.typeParam
                        );

                        if (isErr(normalized)) {
                            return takeErr(std::move(normalized));
                        }

                        result.items.emplace_back(takePrimitive(std::move(normalized)));
                    }

                    return result;
                }

                result.items.reserve(value.arrayOfNullable.count);

                for (std::uint32_t i = 0; i < value.arrayOfNullable.count; ++i) {
                    const auto& item = value.arrayOfNullable.items[i];

                    if (!item.hasValue) {
                        result.items.emplace_back(std::nullopt);
                        continue;
                    }

                    auto normalized = normalizePrimitive(item.value, *innerType.typeParam);
                    if (isErr(normalized)) {
                        return takeErr(std::move(normalized));
                    }

                    result.items.emplace_back(takePrimitive(std::move(normalized)));
                }

                return result;
            }

            if (!isPrimitiveType(innerType)) {
                return schemaMismatch(
                    "Array can contain only primitive or nullable primitive values."
                );
            }

            if (value.kind != ColCmdValueKind::Array) {
                return schemaMismatch("Expected array value without null items.");
            }

            StoredArrayColValue result;
            result.items.reserve(value.array.count);

            for (std::uint32_t i = 0; i < value.array.count; ++i) {
                auto normalized = normalizePrimitive(value.array.items[i], innerType);
                if (isErr(normalized)) {
                    return takeErr(std::move(normalized));
                }

                result.items.push_back(takePrimitive(std::move(normalized)));
            }

            return result;
        }

        return schemaMismatch("Unsupported column type.");
    }

    PrimitiveCmdValue toDtoPrimitive(
        const StoredPrimitive& value,
        ResultDtoStorage& storage
    ) {
        PrimitiveCmdValue result;

        switch (value.kind) {
        case StoredPrimitiveKind::Uuid:
            result.kind = PrimitiveCmdValueKind::Uuid;
            result.uuid = UuidCmdValue{value.uuid};
            return result;

        case StoredPrimitiveKind::CharSeq: {
            auto holder = std::make_shared<std::string>(value.charSeq);

            result.kind = PrimitiveCmdValueKind::CharSeq;
            result.charSeq = CharSeqCmdValue{
                holder->c_str(),
                static_cast<std::uint32_t>(holder->size())
            };

            storage.strings.push_back(std::move(holder));
            return result;
        }

        case StoredPrimitiveKind::Number:
            result.kind = PrimitiveCmdValueKind::Number;
            result.number = NumberCmdValue{};
            result.number.isSigned = value.number.isSigned;
            result.number.byteLength = value.number.byteLength;
            result.number.bytes = {};
            std::copy(
                value.number.bytes.begin(),
                value.number.bytes.end(),
                result.number.bytes.begin()
            );
            return result;

        case StoredPrimitiveKind::Bool:
            result.kind = PrimitiveCmdValueKind::Bool;
            result.boolean = BoolCmdValue{value.boolean};
            return result;

        case StoredPrimitiveKind::Float:
            result.kind = PrimitiveCmdValueKind::Float;
            result.floating = FloatCmdValue{value.floating};
            return result;
        }

        return result;
    }

    ColCmdValue toDtoColValue(
        const StoredColValue& value,
        ResultDtoStorage& storage
    ) {
        ColCmdValue result;
        result.kind = colValueKind(value);

        switch (result.kind) {
        case ColCmdValueKind::Plain: {
            const auto& concreteValue = std::get<StoredPlainColValue>(value);
            result.plain = toDtoPrimitive(concreteValue.value, storage);
            return result;
        }

        case ColCmdValueKind::Nullable: {
            const auto& concreteValue = std::get<StoredNullableColValue>(value);
            result.nullable.hasValue = concreteValue.value.has_value();

            if (concreteValue.value.has_value()) {
                result.nullable.value = toDtoPrimitive(*concreteValue.value, storage);
            }

            return result;
        }

        case ColCmdValueKind::Array: {
            const auto& concreteValue = std::get<StoredArrayColValue>(value);

            auto holder = std::make_shared<std::vector<PrimitiveCmdValue>>();
            holder->reserve(concreteValue.items.size());

            for (const auto& item : concreteValue.items) {
                holder->push_back(toDtoPrimitive(item, storage));
            }

            result.array.items = holder->data();
            result.array.count = static_cast<std::uint32_t>(holder->size());
            storage.primitiveArrays.push_back(std::move(holder));
            return result;
        }

        case ColCmdValueKind::ArrayOfNullable: {
            const auto& concreteValue = std::get<StoredArrayOfNullableColValue>(value);

            auto holder = std::make_shared<std::vector<NullablePrimitiveCmdValue>>();
            holder->reserve(concreteValue.items.size());

            for (const auto& item : concreteValue.items) {
                NullablePrimitiveCmdValue dtoItem;
                dtoItem.hasValue = item.has_value();

                if (item.has_value()) {
                    dtoItem.value = toDtoPrimitive(*item, storage);
                }

                holder->push_back(dtoItem);
            }

            result.arrayOfNullable.items = holder->data();
            result.arrayOfNullable.count = static_cast<std::uint32_t>(holder->size());
            storage.nullablePrimitiveArrays.push_back(std::move(holder));
            return result;
        }

        case ColCmdValueKind::NullableArray: {
            const auto& concreteValue = std::get<StoredNullableArrayColValue>(value);

            result.nullableArray.hasValue = concreteValue.items.has_value();

            if (!concreteValue.items.has_value()) {
                result.nullableArray.value = PrimitiveCmdValueArrayView{};
                return result;
            }

            auto holder = std::make_shared<std::vector<PrimitiveCmdValue>>();
            holder->reserve(concreteValue.items->size());

            for (const auto& item : *concreteValue.items) {
                holder->push_back(toDtoPrimitive(item, storage));
            }

            result.nullableArray.value.items = holder->data();
            result.nullableArray.value.count = static_cast<std::uint32_t>(holder->size());
            storage.primitiveArrays.push_back(std::move(holder));
            return result;
        }

        case ColCmdValueKind::NullableArrayOfNullable: {
            const auto& concreteValue =
                std::get<StoredNullableArrayOfNullableColValue>(value);

            result.nullableArrayOfNullable.hasValue = concreteValue.items.has_value();

            if (!concreteValue.items.has_value()) {
                result.nullableArrayOfNullable.value = NullablePrimitiveCmdValueArrayView{};
                return result;
            }

            auto holder = std::make_shared<std::vector<NullablePrimitiveCmdValue>>();
            holder->reserve(concreteValue.items->size());

            for (const auto& item : *concreteValue.items) {
                NullablePrimitiveCmdValue dtoItem;
                dtoItem.hasValue = item.has_value();

                if (item.has_value()) {
                    dtoItem.value = toDtoPrimitive(*item, storage);
                }

                holder->push_back(dtoItem);
            }

            result.nullableArrayOfNullable.value.items = holder->data();
            result.nullableArrayOfNullable.value.count =
                static_cast<std::uint32_t>(holder->size());
            storage.nullablePrimitiveArrays.push_back(std::move(holder));
            return result;
        }
        }

        return result;
    }
}
