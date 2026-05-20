#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "cmd_values.h"
#include "table_name.h"

namespace kvdb::contracts {
    struct CmdDtoStorage final
    {
        std::vector<std::unique_ptr<CmdTypeKindValue>> typeNodes;
        std::vector<std::shared_ptr<std::string>> strings;
        std::vector<std::shared_ptr<std::vector<PrimitiveCmdValue>>> primitiveArrays;
        std::vector<std::shared_ptr<std::vector<NullablePrimitiveCmdValue>>> nullablePrimitiveArrays;
    };

    enum class CmdKind : std::uint8_t
    {
        CreateTable,
        EraseTable,
        EnsureTableErased,

        Set,
        Get,
        Del,
        EnsureDel,

        Begin,
        Commit,
        Rollback,
        AnyTransaction,

        TableInfo
    };

    struct BaseCmdDto
    {
        virtual ~BaseCmdDto() = default;

        [[nodiscard]]
        virtual CmdKind kind() const noexcept = 0;
    };

    struct CreateTableCmdDto final : BaseCmdDto
    {
        TableName tableName;
        CmdTypeKindValue keyType;
        CmdTypeKindValue valueType;
        CmdDtoStorage storage;

        CreateTableCmdDto(
            TableName tableName,
            CmdTypeKindValue keyType,
            CmdTypeKindValue valueType,
            CmdDtoStorage storage = {}
        )
            : tableName(std::move(tableName)),
              keyType(keyType),
              valueType(valueType),
              storage(std::move(storage)) {}

        [[nodiscard]]
        CmdKind kind() const noexcept override
        {
            return CmdKind::CreateTable;
        }
    };

    struct EraseTableCmdDto final : BaseCmdDto
    {
        TableName tableName;

        explicit EraseTableCmdDto(TableName tableName)
            : tableName(std::move(tableName)) {}

        [[nodiscard]]
        CmdKind kind() const noexcept override
        {
            return CmdKind::EraseTable;
        }
    };

    struct EnsureTableErasedCmdDto final : BaseCmdDto
    {
        TableName tableName;

        explicit EnsureTableErasedCmdDto(TableName tableName)
            : tableName(std::move(tableName)) {}

        [[nodiscard]]
        CmdKind kind() const noexcept override
        {
            return CmdKind::EnsureTableErased;
        }
    };

    struct SetCmdDto final : BaseCmdDto
    {
        TableName tableName;
        KeyCmdValue keyValue;
        ColCmdValue value;
        CmdDtoStorage storage;

        SetCmdDto(
            TableName tableName,
            KeyCmdValue keyValue,
            ColCmdValue value,
            CmdDtoStorage storage = {}
        )
            : tableName(std::move(tableName)),
              keyValue(keyValue),
              value(value),
              storage(std::move(storage)) {}

        [[nodiscard]]
        CmdKind kind() const noexcept override
        {
            return CmdKind::Set;
        }
    };

    struct GetCmdDto final : BaseCmdDto
    {
        TableName tableName;
        KeyCmdValue keyValue;
        CmdDtoStorage storage;

        GetCmdDto(
            TableName tableName,
            KeyCmdValue keyValue,
            CmdDtoStorage storage = {}
        )
            : tableName(std::move(tableName)),
              keyValue(keyValue),
              storage(std::move(storage)) {}

        [[nodiscard]]
        CmdKind kind() const noexcept override
        {
            return CmdKind::Get;
        }
    };

    struct DelCmdDto final : BaseCmdDto
    {
        TableName tableName;
        KeyCmdValue keyValue;
        CmdDtoStorage storage;

        DelCmdDto(
            TableName tableName,
            KeyCmdValue keyValue,
            CmdDtoStorage storage = {}
        )
            : tableName(std::move(tableName)),
              keyValue(keyValue),
              storage(std::move(storage)) {}

        [[nodiscard]]
        CmdKind kind() const noexcept override
        {
            return CmdKind::Del;
        }
    };

    struct EnsureDelCmdDto final : BaseCmdDto
    {
        TableName tableName;
        KeyCmdValue keyValue;
        CmdDtoStorage storage;

        EnsureDelCmdDto(
            TableName tableName,
            KeyCmdValue keyValue,
            CmdDtoStorage storage = {}
        )
            : tableName(std::move(tableName)),
              keyValue(keyValue),
              storage(std::move(storage)) {}

        [[nodiscard]]
        CmdKind kind() const noexcept override
        {
            return CmdKind::EnsureDel;
        }
    };

    struct BeginCmdDto final : BaseCmdDto
    {
        [[nodiscard]]
        CmdKind kind() const noexcept override
        {
            return CmdKind::Begin;
        }
    };

    struct CommitCmdDto final : BaseCmdDto
    {
        [[nodiscard]]
        CmdKind kind() const noexcept override
        {
            return CmdKind::Commit;
        }
    };

    struct RollbackCmdDto final : BaseCmdDto
    {
        [[nodiscard]]
        CmdKind kind() const noexcept override
        {
            return CmdKind::Rollback;
        }
    };

    struct AnyTransactionCmdDto final : BaseCmdDto
    {
        [[nodiscard]]
        CmdKind kind() const noexcept override
        {
            return CmdKind::AnyTransaction;
        }
    };

    struct TableInfoCmdDto final : BaseCmdDto
    {
        TableName tableName;

        explicit TableInfoCmdDto(TableName tableName)
            : tableName(std::move(tableName)) {}

        [[nodiscard]]
        CmdKind kind() const noexcept override
        {
            return CmdKind::TableInfo;
        }
    };
}
