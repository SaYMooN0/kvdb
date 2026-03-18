#pragma once

#include <cstdint>
#include <variant>

#include "cmd_values.h"
#include "schema_type.h"
#include "table_name.h"

namespace kvdb::contracts {
    struct CreateNewTableCmd final
    {
        TableName tableName;
        SchemaType keyType;
        SchemaType valType;
    };

    struct SetInCmd final
    {
        TableName tableName;
        KeyCmdValue keyValue;
        ColCmdValue valValue;
    };

    struct GetInCmd final
    {
        TableName tableName;
        KeyCmdValue keyValue;
    };

    struct DeleteInCmd final
    {
        TableName tableName;
        KeyCmdValue keyValue;
    };

    struct EnsureDeletedInCmd final
    {
        TableName tableName;
        KeyCmdValue keyValue;
    };

    struct EraseTableCmd final
    {
        TableName tableName;
    };

    struct EnsureErasedTableCmd final
    {
        TableName tableName;
    };

    struct TableInfoCmd final
    {
        TableName tableName;
    };

    using Cmd = std::variant<
        CreateNewTableCmd,
        SetInCmd,
        GetInCmd,
        DeleteInCmd,
        EnsureDeletedInCmd,
        EraseTableCmd,
        EnsureErasedTableCmd,
        TableInfoCmd
    >;

    struct EmptyCmdExecResult final {};

    struct AffectedRowsCmdExecResult final
    {
        std::uint64_t affectedRowsCount = 0;
    };

    struct GetInCmdExecResult final
    {
        ColCmdValue value;
    };

    struct TableInfoCmdExecResult final
    {
        TableName tableName;
        SchemaType keyType;
        SchemaType valType;
        std::uint64_t rowsCount = 0;
    };

    using CmdExecResult = std::variant<
        EmptyCmdExecResult,
        AffectedRowsCmdExecResult,
        GetInCmdExecResult,
        TableInfoCmdExecResult
    >;
}
