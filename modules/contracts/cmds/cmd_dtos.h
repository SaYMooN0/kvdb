#pragma once

#include <cstdint>

#include "cmd_values.h"
#include "table_name.h"

namespace kvdb::contracts {
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

        [[nodiscard]]
        CmdKind kind() const noexcept override
        {
            return CmdKind::CreateTable;
        }
    };

    struct EraseTableCmdDto final : BaseCmdDto
    {
        TableName tableName;

        [[nodiscard]]
        CmdKind kind() const noexcept override
        {
            return CmdKind::EraseTable;
        }
    };

    struct EnsureTableErasedCmdDto final : BaseCmdDto
    {
        TableName tableName;

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

        [[nodiscard]]
        CmdKind kind() const noexcept override
        {
            return CmdKind::TableInfo;
        }
    };
}