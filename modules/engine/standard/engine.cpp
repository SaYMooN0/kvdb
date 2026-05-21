#include "i_modules.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace kvdb::modules::engine::standard {
    namespace {
        using namespace kvdb::contracts;

        constexpr const char* StateFilePath = "kvdb_engine_state.bin";

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

        CmdExecErr makeErr(std::string code, std::string message) {
            return std::make_shared<EngineErr>(std::move(code), std::move(message));
        }

        CmdExecResult makeSuccess(SuccessCmdExecResult success) {
            return success;
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

        struct DbType final
        {
            CmdTypeKind type = CmdTypeKind::Bool;
            std::uint16_t sizeParam = 0;
            std::shared_ptr<const DbType> typeParam;
        };

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

        struct StoredNumber final
        {
            bool isSigned = false;
            std::uint8_t byteLength = 0;
            std::array<std::uint8_t, 16> bytes{};
        };

        enum class StoredPrimitiveKind : std::uint8_t
        {
            Uuid,
            CharSeq,
            Number,
            Bool,
            Float
        };

        struct StoredPrimitive final
        {
            StoredPrimitiveKind kind = StoredPrimitiveKind::Bool;

            std::array<std::uint8_t, 16> uuid{};
            std::string charSeq;
            StoredNumber number;
            bool boolean = false;
            double floating = 0.0;
        };

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

        struct StoredPrimitiveHash final
        {
            std::size_t operator()(const StoredPrimitive& value) const {
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
                        for (const auto byte : value.number.bytes) {
                            hashCombine(seed, std::hash<std::uint8_t>{}(byte));
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
        };

        struct StoredColValue final
        {
            ColCmdValueKind kind = ColCmdValueKind::Plain;

            StoredPrimitive plain;

            std::optional<StoredPrimitive> nullable;

            std::vector<StoredPrimitive> array;
            std::vector<std::optional<StoredPrimitive>> arrayOfNullable;

            bool nullableArrayHasValue = false;
            std::vector<StoredPrimitive> nullableArray;

            bool nullableArrayOfNullableHasValue = false;
            std::vector<std::optional<StoredPrimitive>> nullableArrayOfNullable;
        };

        struct Table final
        {
            DbType keyType;
            DbType valueType;

            std::unordered_map<
                StoredPrimitive,
                StoredColValue,
                StoredPrimitiveHash
            > rows;
        };

        using Tables = std::unordered_map<std::string, Table>;

        struct ResultDtoStorage final
        {
            std::vector<std::shared_ptr<std::string>> strings;
            std::vector<std::shared_ptr<std::vector<PrimitiveCmdValue>>> primitiveArrays;
            std::vector<std::shared_ptr<std::vector<NullablePrimitiveCmdValue>>> nullablePrimitiveArrays;
            std::vector<std::unique_ptr<CmdTypeKindValue>> typeNodes;

            void clear() {
                strings.clear();
                primitiveArrays.clear();
                nullablePrimitiveArrays.clear();
                typeNodes.clear();
            }
        };

        std::uint64_t readUnsignedNumber(const NumberCmdValue& value) {
            const auto byteLength = static_cast<std::size_t>(value.byteLength);
            if (byteLength > 8) {
                throw std::runtime_error("Numbers larger than 8 bytes are not supported by standard engine.");
            }

            std::uint64_t result = 0;
            for (std::size_t i = 0; i < byteLength; ++i) {
                result |= static_cast<std::uint64_t>(value.bytes[i]) << (i * 8U);
            }

            return result;
        }

        std::int64_t readSignedNumber(const NumberCmdValue& value) {
            const auto byteLength = static_cast<std::size_t>(value.byteLength);
            if (byteLength == 0 || byteLength > 8) {
                throw std::runtime_error("Invalid signed number byte length.");
            }

            auto raw = readUnsignedNumber(value);

            const auto signBit = std::uint64_t{1} << (byteLength * 8U - 1U);
            if ((raw & signBit) != 0 && byteLength < 8) {
                const auto extensionMask = ~((std::uint64_t{1} << (byteLength * 8U)) - 1U);
                raw |= extensionMask;
            }

            return static_cast<std::int64_t>(raw);
        }

        StoredNumber makeStoredNumberFromUnsigned(std::uint64_t value) {
            StoredNumber result;
            result.isSigned = false;
            result.byteLength = 8;

            for (std::size_t i = 0; i < 8; ++i) {
                result.bytes[i] = static_cast<std::uint8_t>((value >> (i * 8U)) & 0xFFU);
            }

            return result;
        }

        StoredNumber makeStoredNumberFromSigned(std::int64_t value) {
            StoredNumber result;
            result.isSigned = true;
            result.byteLength = 8;

            const auto raw = static_cast<std::uint64_t>(value);
            for (std::size_t i = 0; i < 8; ++i) {
                result.bytes[i] = static_cast<std::uint8_t>((raw >> (i * 8U)) & 0xFFU);
            }

            return result;
        }

        CmdExecErr schemaMismatch(std::string message) {
            return makeErr("SchemaMismatch", std::move(message));
        }

        CmdExecErr invalidValue(std::string message) {
            return makeErr("InvalidValue", std::move(message));
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
                        return schemaMismatch("Charseq value is longer than the declared charseq size.");
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

                    StoredPrimitive result;
                    result.kind = StoredPrimitiveKind::Number;

                    try {
                        if (value.number.isSigned) {
                            result.number = makeStoredNumberFromSigned(readSignedNumber(value.number));
                        }
                        else {
                            const auto unsignedValue = readUnsignedNumber(value.number);
                            if (unsignedValue > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
                                return invalidValue("Unsigned number does not fit into int.");
                            }

                            result.number = makeStoredNumberFromSigned(static_cast<std::int64_t>(unsignedValue));
                        }
                    }
                    catch (const std::exception& ex) {
                        return invalidValue(ex.what());
                    }

                    return result;
                }

                case CmdTypeKind::UInt: {
                    if (value.kind != PrimitiveCmdValueKind::Number) {
                        return schemaMismatch("Expected uint value.");
                    }

                    StoredPrimitive result;
                    result.kind = StoredPrimitiveKind::Number;

                    try {
                        if (value.number.isSigned) {
                            const auto signedValue = readSignedNumber(value.number);
                            if (signedValue < 0) {
                                return invalidValue("Negative number cannot be used as uint.");
                            }

                            result.number = makeStoredNumberFromUnsigned(static_cast<std::uint64_t>(signedValue));
                        }
                        else {
                            result.number = makeStoredNumberFromUnsigned(readUnsignedNumber(value.number));
                        }
                    }
                    catch (const std::exception& ex) {
                        return invalidValue(ex.what());
                    }

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
                                result.floating = static_cast<double>(readSignedNumber(value.number));
                            }
                            else {
                                result.floating = static_cast<double>(readUnsignedNumber(value.number));
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

                StoredColValue result;
                result.kind = ColCmdValueKind::Plain;
                result.plain = takePrimitive(std::move(normalized));
                return result;
            }

            if (expectedType.type == CmdTypeKind::Nullable) {
                if (!expectedType.typeParam) {
                    return schemaMismatch("Nullable type does not have inner type.");
                }

                const auto& innerType = *expectedType.typeParam;

                if (value.kind == ColCmdValueKind::Nullable && !value.nullable.hasValue) {
                    StoredColValue result;

                    if (innerType.type == CmdTypeKind::Array) {
                        if (!innerType.typeParam) {
                            return schemaMismatch("Array type does not have inner type.");
                        }

                        if (innerType.typeParam->type == CmdTypeKind::Nullable) {
                            result.kind = ColCmdValueKind::NullableArrayOfNullable;
                            result.nullableArrayOfNullableHasValue = false;
                        }
                        else {
                            result.kind = ColCmdValueKind::NullableArray;
                            result.nullableArrayHasValue = false;
                        }
                    }
                    else {
                        result.kind = ColCmdValueKind::Nullable;
                        result.nullable = std::nullopt;
                    }

                    return result;
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

                    StoredColValue result;
                    result.kind = ColCmdValueKind::Nullable;
                    result.nullable = takePrimitive(std::move(normalized));
                    return result;
                }

                if (innerType.type == CmdTypeKind::Array) {
                    auto normalizedArray = normalizeColValue(value, innerType);
                    if (std::holds_alternative<CmdExecErr>(normalizedArray)) {
                        return std::get<CmdExecErr>(std::move(normalizedArray));
                    }

                    auto arrayValue = std::get<StoredColValue>(std::move(normalizedArray));

                    StoredColValue result;

                    if (arrayValue.kind == ColCmdValueKind::Array) {
                        result.kind = ColCmdValueKind::NullableArray;
                        result.nullableArrayHasValue = true;
                        result.nullableArray = std::move(arrayValue.array);
                        return result;
                    }

                    if (arrayValue.kind == ColCmdValueKind::ArrayOfNullable) {
                        result.kind = ColCmdValueKind::NullableArrayOfNullable;
                        result.nullableArrayOfNullableHasValue = true;
                        result.nullableArrayOfNullable = std::move(arrayValue.arrayOfNullable);
                        return result;
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

                    if (value.kind != ColCmdValueKind::Array && value.kind != ColCmdValueKind::ArrayOfNullable) {
                        return schemaMismatch("Expected array value.");
                    }

                    StoredColValue result;
                    result.kind = ColCmdValueKind::ArrayOfNullable;

                    if (value.kind == ColCmdValueKind::Array) {
                        for (std::uint32_t i = 0; i < value.array.count; ++i) {
                            auto normalized = normalizePrimitive(value.array.items[i], *innerType.typeParam);
                            if (isErr(normalized)) {
                                return takeErr(std::move(normalized));
                            }

                            result.arrayOfNullable.emplace_back(takePrimitive(std::move(normalized)));
                        }

                        return result;
                    }

                    for (std::uint32_t i = 0; i < value.arrayOfNullable.count; ++i) {
                        const auto& item = value.arrayOfNullable.items[i];

                        if (!item.hasValue) {
                            result.arrayOfNullable.emplace_back(std::nullopt);
                            continue;
                        }

                        auto normalized = normalizePrimitive(item.value, *innerType.typeParam);
                        if (isErr(normalized)) {
                            return takeErr(std::move(normalized));
                        }

                        result.arrayOfNullable.emplace_back(takePrimitive(std::move(normalized)));
                    }

                    return result;
                }

                if (!isPrimitiveType(innerType)) {
                    return schemaMismatch("Array can contain only primitive or nullable primitive values.");
                }

                if (value.kind != ColCmdValueKind::Array) {
                    return schemaMismatch("Expected array value without null items.");
                }

                StoredColValue result;
                result.kind = ColCmdValueKind::Array;

                for (std::uint32_t i = 0; i < value.array.count; ++i) {
                    auto normalized = normalizePrimitive(value.array.items[i], innerType);
                    if (isErr(normalized)) {
                        return takeErr(std::move(normalized));
                    }

                    result.array.push_back(takePrimitive(std::move(normalized)));
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
                    result.number.bytes = value.number.bytes;
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
            result.kind = value.kind;

            switch (value.kind) {
                case ColCmdValueKind::Plain:
                    result.plain = toDtoPrimitive(value.plain, storage);
                    return result;

                case ColCmdValueKind::Nullable:
                    result.nullable.hasValue = value.nullable.has_value();
                    if (value.nullable.has_value()) {
                        result.nullable.value = toDtoPrimitive(*value.nullable, storage);
                    }
                    return result;

                case ColCmdValueKind::Array: {
                    auto holder = std::make_shared<std::vector<PrimitiveCmdValue>>();
                    holder->reserve(value.array.size());

                    for (const auto& item : value.array) {
                        holder->push_back(toDtoPrimitive(item, storage));
                    }

                    result.array.items = holder->data();
                    result.array.count = static_cast<std::uint32_t>(holder->size());
                    storage.primitiveArrays.push_back(std::move(holder));
                    return result;
                }

                case ColCmdValueKind::ArrayOfNullable: {
                    auto holder = std::make_shared<std::vector<NullablePrimitiveCmdValue>>();
                    holder->reserve(value.arrayOfNullable.size());

                    for (const auto& item : value.arrayOfNullable) {
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
                    result.nullableArray.hasValue = value.nullableArrayHasValue;

                    if (!value.nullableArrayHasValue) {
                        result.nullableArray.value = PrimitiveCmdValueArrayView{};
                        return result;
                    }

                    auto holder = std::make_shared<std::vector<PrimitiveCmdValue>>();
                    holder->reserve(value.nullableArray.size());

                    for (const auto& item : value.nullableArray) {
                        holder->push_back(toDtoPrimitive(item, storage));
                    }

                    result.nullableArray.value.items = holder->data();
                    result.nullableArray.value.count = static_cast<std::uint32_t>(holder->size());
                    storage.primitiveArrays.push_back(std::move(holder));
                    return result;
                }

                case ColCmdValueKind::NullableArrayOfNullable: {
                    result.nullableArrayOfNullable.hasValue = value.nullableArrayOfNullableHasValue;

                    if (!value.nullableArrayOfNullableHasValue) {
                        result.nullableArrayOfNullable.value = NullablePrimitiveCmdValueArrayView{};
                        return result;
                    }

                    auto holder = std::make_shared<std::vector<NullablePrimitiveCmdValue>>();
                    holder->reserve(value.nullableArrayOfNullable.size());

                    for (const auto& item : value.nullableArrayOfNullable) {
                        NullablePrimitiveCmdValue dtoItem;
                        dtoItem.hasValue = item.has_value();

                        if (item.has_value()) {
                            dtoItem.value = toDtoPrimitive(*item, storage);
                        }

                        holder->push_back(dtoItem);
                    }

                    result.nullableArrayOfNullable.value.items = holder->data();
                    result.nullableArrayOfNullable.value.count = static_cast<std::uint32_t>(holder->size());
                    storage.nullablePrimitiveArrays.push_back(std::move(holder));
                    return result;
                }
            }

            return result;
        }

        template <typename T>
        void writePod(std::ostream& out, const T& value) {
            out.write(reinterpret_cast<const char*>(&value), sizeof(T));
            if (!out) {
                throw std::runtime_error("Failed to write engine state.");
            }
        }

        template <typename T>
        T readPod(std::istream& in) {
            T value{};
            in.read(reinterpret_cast<char*>(&value), sizeof(T));
            if (!in) {
                throw std::runtime_error("Failed to read engine state.");
            }

            return value;
        }

        void writeString(std::ostream& out, const std::string& value) {
            const auto size = value.size();
            writePod(out, size);

            out.write(value.data(), static_cast<std::streamsize>(value.size()));
            if (!out) {
                throw std::runtime_error("Failed to write string to engine state.");
            }
        }

        std::string readString(std::istream& in) {
            const auto size = readPod<std::uint64_t>(in);

            if (size > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
                throw std::runtime_error("String in engine state is too large.");
            }

            std::string result(static_cast<std::size_t>(size), '\0');

            if (!result.empty()) {
                in.read(result.data(), static_cast<std::streamsize>(result.size()));
                if (!in) {
                    throw std::runtime_error("Failed to read string from engine state.");
                }
            }

            return result;
        }

        void writeType(std::ostream& out, const DbType& type) {
            writePod(out, static_cast<std::uint8_t>(type.type));
            writePod(out, type.sizeParam);

            const bool hasTypeParam = type.typeParam != nullptr;
            writePod(out, hasTypeParam);

            if (hasTypeParam) {
                writeType(out, *type.typeParam);
            }
        }

        DbType readType(std::istream& in) {
            DbType result;
            result.type = static_cast<CmdTypeKind>(readPod<std::uint8_t>(in));
            result.sizeParam = readPod<std::uint16_t>(in);

            const bool hasTypeParam = readPod<bool>(in);
            if (hasTypeParam) {
                result.typeParam = std::make_shared<DbType>(readType(in));
            }

            return result;
        }

        void writePrimitive(std::ostream& out, const StoredPrimitive& value) {
            writePod(out, static_cast<std::uint8_t>(value.kind));

            switch (value.kind) {
                case StoredPrimitiveKind::Uuid:
                    out.write(reinterpret_cast<const char*>(value.uuid.data()), static_cast<std::streamsize>(value.uuid.size()));
                    if (!out) {
                        throw std::runtime_error("Failed to write uuid.");
                    }
                    break;

                case StoredPrimitiveKind::CharSeq:
                    writeString(out, value.charSeq);
                    break;

                case StoredPrimitiveKind::Number:
                    writePod(out, value.number.isSigned);
                    writePod(out, value.number.byteLength);
                    out.write(reinterpret_cast<const char*>(value.number.bytes.data()), value.number.bytes.size());
                    if (!out) {
                        throw std::runtime_error("Failed to write number.");
                    }
                    break;

                case StoredPrimitiveKind::Bool:
                    writePod(out, value.boolean);
                    break;

                case StoredPrimitiveKind::Float:
                    writePod(out, value.floating);
                    break;
            }
        }

        StoredPrimitive readPrimitive(std::istream& in) {
            StoredPrimitive result;
            result.kind = static_cast<StoredPrimitiveKind>(readPod<std::uint8_t>(in));

            switch (result.kind) {
                case StoredPrimitiveKind::Uuid:
                    in.read(reinterpret_cast<char*>(result.uuid.data()), static_cast<std::streamsize>(result.uuid.size()));
                    if (!in) {
                        throw std::runtime_error("Failed to read uuid.");
                    }
                    break;

                case StoredPrimitiveKind::CharSeq:
                    result.charSeq = readString(in);
                    break;

                case StoredPrimitiveKind::Number:
                    result.number.isSigned = readPod<bool>(in);
                    result.number.byteLength = readPod<std::uint8_t>(in);
                    in.read(reinterpret_cast<char*>(result.number.bytes.data()), static_cast<std::streamsize>(result.number.bytes.size()));
                    if (!in) {
                        throw std::runtime_error("Failed to read number.");
                    }
                    break;

                case StoredPrimitiveKind::Bool:
                    result.boolean = readPod<bool>(in);
                    break;

                case StoredPrimitiveKind::Float:
                    result.floating = readPod<double>(in);
                    break;
            }

            return result;
        }

        void writePrimitiveVector(std::ostream& out, const std::vector<StoredPrimitive>& values) {
            writePod(out, static_cast<std::uint64_t>(values.size()));

            for (const auto& value : values) {
                writePrimitive(out, value);
            }
        }

        std::vector<StoredPrimitive> readPrimitiveVector(std::istream& in) {
            const auto count = readPod<std::uint64_t>(in);

            std::vector<StoredPrimitive> result;
            result.reserve(static_cast<std::size_t>(count));

            for (std::uint64_t i = 0; i < count; ++i) {
                result.push_back(readPrimitive(in));
            }

            return result;
        }

        void writeNullablePrimitiveVector(
            std::ostream& out,
            const std::vector<std::optional<StoredPrimitive>>& values
        ) {
            writePod(out, static_cast<std::uint64_t>(values.size()));

            for (const auto& value : values) {
                const bool hasValue = value.has_value();
                writePod(out, hasValue);

                if (hasValue) {
                    writePrimitive(out, *value);
                }
            }
        }

        std::vector<std::optional<StoredPrimitive>> readNullablePrimitiveVector(std::istream& in) {
            const auto count = readPod<std::uint64_t>(in);

            std::vector<std::optional<StoredPrimitive>> result;
            result.reserve(static_cast<std::size_t>(count));

            for (std::uint64_t i = 0; i < count; ++i) {
                const bool hasValue = readPod<bool>(in);

                if (hasValue) {
                    result.emplace_back(readPrimitive(in));
                }
                else {
                    result.emplace_back(std::nullopt);
                }
            }

            return result;
        }

        void writeColValue(std::ostream& out, const StoredColValue& value) {
            writePod(out, static_cast<std::uint8_t>(value.kind));

            switch (value.kind) {
                case ColCmdValueKind::Plain:
                    writePrimitive(out, value.plain);
                    break;

                case ColCmdValueKind::Nullable: {
                    const bool hasValue = value.nullable.has_value();
                    writePod(out, hasValue);

                    if (hasValue) {
                        writePrimitive(out, *value.nullable);
                    }

                    break;
                }

                case ColCmdValueKind::Array:
                    writePrimitiveVector(out, value.array);
                    break;

                case ColCmdValueKind::ArrayOfNullable:
                    writeNullablePrimitiveVector(out, value.arrayOfNullable);
                    break;

                case ColCmdValueKind::NullableArray:
                    writePod(out, value.nullableArrayHasValue);
                    if (value.nullableArrayHasValue) {
                        writePrimitiveVector(out, value.nullableArray);
                    }
                    break;

                case ColCmdValueKind::NullableArrayOfNullable:
                    writePod(out, value.nullableArrayOfNullableHasValue);
                    if (value.nullableArrayOfNullableHasValue) {
                        writeNullablePrimitiveVector(out, value.nullableArrayOfNullable);
                    }
                    break;
            }
        }

        StoredColValue readColValue(std::istream& in) {
            StoredColValue result;
            result.kind = static_cast<ColCmdValueKind>(readPod<std::uint8_t>(in));

            switch (result.kind) {
                case ColCmdValueKind::Plain:
                    result.plain = readPrimitive(in);
                    break;

                case ColCmdValueKind::Nullable: {
                    const bool hasValue = readPod<bool>(in);
                    if (hasValue) {
                        result.nullable = readPrimitive(in);
                    }
                    else {
                        result.nullable = std::nullopt;
                    }
                    break;
                }

                case ColCmdValueKind::Array:
                    result.array = readPrimitiveVector(in);
                    break;

                case ColCmdValueKind::ArrayOfNullable:
                    result.arrayOfNullable = readNullablePrimitiveVector(in);
                    break;

                case ColCmdValueKind::NullableArray:
                    result.nullableArrayHasValue = readPod<bool>(in);
                    if (result.nullableArrayHasValue) {
                        result.nullableArray = readPrimitiveVector(in);
                    }
                    break;

                case ColCmdValueKind::NullableArrayOfNullable:
                    result.nullableArrayOfNullableHasValue = readPod<bool>(in);
                    if (result.nullableArrayOfNullableHasValue) {
                        result.nullableArrayOfNullable = readNullablePrimitiveVector(in);
                    }
                    break;
            }

            return result;
        }

        void saveTablesToFile(const Tables& tables) {
            std::ofstream out(StateFilePath, std::ios::binary | std::ios::trunc);

            if (!out) {
                throw std::runtime_error("Unable to open engine state file for writing.");
            }

            writeString(out, "KVDB_ENGINE_STATE_V1");
            writePod(out, static_cast<std::uint64_t>(tables.size()));

            for (const auto& [tableName, table] : tables) {
                writeString(out, tableName);
                writeType(out, table.keyType);
                writeType(out, table.valueType);

                writePod(out, static_cast<std::uint64_t>(table.rows.size()));

                for (const auto& [key, value] : table.rows) {
                    writePrimitive(out, key);
                    writeColValue(out, value);
                }
            }
        }

        Tables loadTablesFromFile() {
            std::ifstream in(StateFilePath, std::ios::binary);

            if (!in) {
                return {};
            }

            const auto magic = readString(in);
            if (magic != "KVDB_ENGINE_STATE_V1") {
                throw std::runtime_error("Invalid engine state file format.");
            }

            Tables result;

            const auto tableCount = readPod<std::uint64_t>(in);

            for (std::uint64_t i = 0; i < tableCount; ++i) {
                auto tableName = readString(in);

                Table table;
                table.keyType = readType(in);
                table.valueType = readType(in);

                const auto rowCount = readPod<std::uint64_t>(in);

                for (std::uint64_t row = 0; row < rowCount; ++row) {
                    auto key = readPrimitive(in);
                    auto value = readColValue(in);

                    table.rows.emplace(std::move(key), std::move(value));
                }

                result.emplace(std::move(tableName), std::move(table));
            }

            return result;
        }
    }

    class StandardEngine final : public kvdb::contracts::IEngine
    {
    public:
        kvdb::contracts::CmdExecResult execute(
            const kvdb::contracts::BaseCmdDto& cmd
        ) override {
            resultStorage_.clear();

            using namespace kvdb::contracts;

            switch (cmd.kind()) {
                case CmdKind::Begin:
                    return executeBegin();

                case CmdKind::Commit:
                    return executeCommit();

                case CmdKind::Rollback:
                    return executeRollback();

                case CmdKind::AnyTransaction:
                    return SuccessCmdExecResult{
                        TransactionCheckCmdExecSuccess{
                            .transactionActive = transactionActive_
                        }
                    };

                case CmdKind::CreateTable:
                    return executeCreateTable(static_cast<const CreateTableCmdDto&>(cmd));

                case CmdKind::EraseTable:
                    return executeEraseTable(static_cast<const EraseTableCmdDto&>(cmd));

                case CmdKind::EnsureTableErased:
                    return executeEnsureTableErased(static_cast<const EnsureTableErasedCmdDto&>(cmd));

                case CmdKind::Set:
                    return executeSet(static_cast<const SetCmdDto&>(cmd));

                case CmdKind::Get:
                    return executeGet(static_cast<const GetCmdDto&>(cmd));

                case CmdKind::Del:
                    return executeDel(static_cast<const DelCmdDto&>(cmd));

                case CmdKind::EnsureDel:
                    return executeEnsureDel(static_cast<const EnsureDelCmdDto&>(cmd));

                case CmdKind::TableInfo:
                    return executeTableInfo(static_cast<const TableInfoCmdDto&>(cmd));
            }

            return makeErr("UnknownCommand", "Unsupported command kind.");
        }

        void onInstanceStart() override {
            committedTables_ = loadTablesFromFile();
            transactionTables_.reset();
            transactionActive_ = false;
        }

        void onInstanceShutdown() override {
            saveTablesToFile(committedTables_);
        }

    private:
        Tables& activeTables() {
            if (transactionActive_) {
                return *transactionTables_;
            }

            return committedTables_;
        }

        const Tables& activeTables() const {
            if (transactionActive_) {
                return *transactionTables_;
            }

            return committedTables_;
        }

        CmdExecResult executeBegin() {
            if (transactionActive_) {
                return makeErr(
                    "TransactionAlreadyActive",
                    "Cannot begin a transaction because another transaction is already active."
                );
            }

            transactionTables_ = committedTables_;
            transactionActive_ = true;

            return makeTransactionSuccess(TransactionOpKind::Begin);
        }

        CmdExecResult executeCommit() {
            if (!transactionActive_) {
                return makeErr(
                    "NoActiveTransaction",
                    "Cannot commit because there is no active transaction."
                );
            }

            committedTables_ = std::move(*transactionTables_);
            transactionTables_.reset();
            transactionActive_ = false;

            return makeTransactionSuccess(TransactionOpKind::Commit);
        }

        CmdExecResult executeRollback() {
            if (!transactionActive_) {
                return makeErr(
                    "NoActiveTransaction",
                    "Cannot rollback because there is no active transaction."
                );
            }

            transactionTables_.reset();
            transactionActive_ = false;

            return makeTransactionSuccess(TransactionOpKind::Rollback);
        }

        CmdExecResult executeCreateTable(const CreateTableCmdDto& cmd) {
            auto& tables = activeTables();
            const auto& tableName = cmd.tableName.value();

            if (tables.contains(tableName)) {
                return makeErr(
                    "TableAlreadyExists",
                    "Table '" + tableName + "' already exists."
                );
            }

            Table table;
            table.keyType = copyType(cmd.keyType);
            table.valueType = copyType(cmd.valueType);

            if (table.keyType.type == CmdTypeKind::Nullable) {
                return makeErr("InvalidKeyType", "Key type cannot be nullable.");
            }

            if (table.keyType.type == CmdTypeKind::Array) {
                return makeErr("InvalidKeyType", "Key type cannot be an array.");
            }

            tables.emplace(tableName, std::move(table));

            return makeEmptySuccess();
        }

        CmdExecResult executeEraseTable(const EraseTableCmdDto& cmd) {
            auto& tables = activeTables();
            const auto& tableName = cmd.tableName.value();

            const auto erased = tables.erase(tableName);
            if (erased == 0) {
                return makeErr(
                    "TableNotFound",
                    "Table '" + tableName + "' does not exist."
                );
            }

            return makeEmptySuccess();
        }

        CmdExecResult executeEnsureTableErased(const EnsureTableErasedCmdDto& cmd) {
            activeTables().erase(cmd.tableName.value());
            return makeEmptySuccess();
        }

        CmdExecResult executeSet(const SetCmdDto& cmd) {
            auto& tables = activeTables();
            const auto& tableName = cmd.tableName.value();

            auto tableIt = tables.find(tableName);
            if (tableIt == tables.end()) {
                return makeErr(
                    "TableNotFound",
                    "Table '" + tableName + "' does not exist."
                );
            }

            auto normalizedKey = normalizePrimitive(cmd.keyValue, tableIt->second.keyType);
            if (std::holds_alternative<CmdExecErr>(normalizedKey)) {
                return std::get<CmdExecErr>(std::move(normalizedKey));
            }

            auto normalizedValue = normalizeColValue(cmd.value, tableIt->second.valueType);
            if (std::holds_alternative<CmdExecErr>(normalizedValue)) {
                return std::get<CmdExecErr>(std::move(normalizedValue));
            }

            tableIt->second.rows[std::get<StoredPrimitive>(std::move(normalizedKey))]
                = std::get<StoredColValue>(std::move(normalizedValue));

            return makeAffectedRowsSuccess(1);
        }

        CmdExecResult executeGet(const GetCmdDto& cmd) {
            const auto& tables = activeTables();
            const auto& tableName = cmd.tableName.value();

            const auto tableIt = tables.find(tableName);
            if (tableIt == tables.end()) {
                return makeErr(
                    "TableNotFound",
                    "Table '" + tableName + "' does not exist."
                );
            }

            auto normalizedKey = normalizePrimitive(cmd.keyValue, tableIt->second.keyType);
            if (std::holds_alternative<CmdExecErr>(normalizedKey)) {
                return std::get<CmdExecErr>(std::move(normalizedKey));
            }

            const auto rowIt = tableIt->second.rows.find(
                std::get<StoredPrimitive>(std::move(normalizedKey))
            );

            if (rowIt == tableIt->second.rows.end()) {
                return makeErr(
                    "KeyNotFound",
                    "Key was not found in table '" + tableName + "'."
                );
            }

            return SuccessCmdExecResult{
                GetCmdExecSuccess{
                    .value = toDtoColValue(rowIt->second, resultStorage_)
                }
            };
        }

        CmdExecResult executeDel(const DelCmdDto& cmd) {
            auto& tables = activeTables();
            const auto& tableName = cmd.tableName.value();

            auto tableIt = tables.find(tableName);
            if (tableIt == tables.end()) {
                return makeErr(
                    "TableNotFound",
                    "Table '" + tableName + "' does not exist."
                );
            }

            auto normalizedKey = normalizePrimitive(cmd.keyValue, tableIt->second.keyType);
            if (std::holds_alternative<CmdExecErr>(normalizedKey)) {
                return std::get<CmdExecErr>(std::move(normalizedKey));
            }

            const auto erased = tableIt->second.rows.erase(
                std::get<StoredPrimitive>(std::move(normalizedKey))
            );

            if (erased == 0) {
                return makeErr(
                    "KeyNotFound",
                    "Key was not found in table '" + tableName + "'."
                );
            }

            return makeAffectedRowsSuccess(1);
        }

        CmdExecResult executeEnsureDel(const EnsureDelCmdDto& cmd) {
            auto& tables = activeTables();
            const auto& tableName = cmd.tableName.value();

            auto tableIt = tables.find(tableName);
            if (tableIt == tables.end()) {
                return makeErr(
                    "TableNotFound",
                    "Table '" + tableName + "' does not exist."
                );
            }

            auto normalizedKey = normalizePrimitive(cmd.keyValue, tableIt->second.keyType);
            if (std::holds_alternative<CmdExecErr>(normalizedKey)) {
                return std::get<CmdExecErr>(std::move(normalizedKey));
            }

            const auto erased = tableIt->second.rows.erase(
                std::get<StoredPrimitive>(std::move(normalizedKey))
            );

            return makeAffectedRowsSuccess(erased);
        }

        CmdExecResult executeTableInfo(const TableInfoCmdDto& cmd) {
            const auto& tables = activeTables();
            const auto& tableName = cmd.tableName.value();

            const auto tableIt = tables.find(tableName);
            if (tableIt == tables.end()) {
                return makeErr(
                    "TableNotFound",
                    "Table '" + tableName + "' does not exist."
                );
            }

            return makeErr(
                "NotImplemented",
                "TableInfo is not implemented in this standard engine version."
            );
        }

    private:
        Tables committedTables_;
        std::optional<Tables> transactionTables_;
        bool transactionActive_ = false;

        ResultDtoStorage resultStorage_;
    };
}

extern "C" __declspec(dllexport)
kvdb::contracts::IEngine* create_engine() {
    return new kvdb::modules::engine::standard::StandardEngine();
}

extern "C" __declspec(dllexport)
void destroy_engine(kvdb::contracts::IEngine* ptr) {
    delete ptr;
}