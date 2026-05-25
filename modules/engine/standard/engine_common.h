#pragma once

#include "i_modules.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace kvdb::modules::engine::standard {
    using namespace kvdb::contracts;

    constexpr const char* StateFilePath = "kvdb_engine_state.bin";
    constexpr std::uint8_t MaxNumberByteLength = 16;

    CmdExecErr makeErr(std::string code, std::string message);
    CmdExecResult makeEmptySuccess();
    CmdExecResult makeTransactionSuccess(TransactionOpKind operation);
    CmdExecResult makeAffectedRowsSuccess(std::uint64_t count);

    struct DbType final
    {
        CmdTypeKind type = CmdTypeKind::Bool;
        std::uint16_t sizeParam = 0;
        std::shared_ptr<const DbType> typeParam;
    };

    DbType copyType(const CmdTypeKindValue& value);
    bool isPrimitiveType(const DbType& type);

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

    std::uint64_t doubleToBits(double value);
    bool operator==(const StoredNumber& left, const StoredNumber& right);
    bool operator==(const StoredPrimitive& left, const StoredPrimitive& right);
    void hashCombine(std::size_t& seed, std::size_t value);

    struct StoredPrimitiveHash final
    {
        std::size_t operator()(const StoredPrimitive& value) const;
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

        std::unordered_map<StoredPrimitive, StoredColValue, StoredPrimitiveHash> rows;
    };

    using TablePtr = std::shared_ptr<Table>;
    using Tables = std::unordered_map<std::string, TablePtr>;

    struct ResultDtoStorage final
    {
        std::vector<std::shared_ptr<std::string>> strings;
        std::vector<std::shared_ptr<std::vector<PrimitiveCmdValue>>> primitiveArrays;
        std::vector<std::shared_ptr<std::vector<NullablePrimitiveCmdValue>>> nullablePrimitiveArrays;
        std::vector<std::unique_ptr<CmdTypeKindValue>> typeNodes;

        void clear();
    };

    CmdTypeKindValue toDtoType(const DbType& type, ResultDtoStorage& storage);

    CmdExecErr schemaMismatch(std::string message);
    CmdExecErr invalidValue(std::string message);

    std::optional<CmdExecErr> validateTableSchema(const Table& table);

    std::variant<StoredPrimitive, CmdExecErr> normalizePrimitive(
        const PrimitiveCmdValue& value,
        const DbType& expectedType
    );

    std::variant<StoredColValue, CmdExecErr> normalizeColValue(
        const ColCmdValue& value,
        const DbType& expectedType
    );

    ColCmdValue toDtoColValue(const StoredColValue& value, ResultDtoStorage& storage);

    void saveTablesToFile(const Tables& tables);
    Tables loadTablesFromFile();
}
