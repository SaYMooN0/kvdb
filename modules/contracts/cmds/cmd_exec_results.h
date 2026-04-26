#pragma once

#include <memory>
#include <variant>

#include "cmd_values.h"
#include "err.h"
#include "table_name.h"

namespace kvdb::contracts {
    enum class TransactionOpKind : std::uint8_t
    {
        Begin,
        Commit,
        Rollback
    };

    struct EmptyCmdExecSuccess final {};

    struct TransactionOpCmdExecSuccess final
    {
        TransactionOpKind operation;
    };

    struct TransactionCheckCmdExecSuccess final
    {
        bool transactionActive = false;
    };

    struct AffectedRowsCmdExecSuccess final
    {
        std::uint64_t count = 0;
    };

    struct GetCmdExecSuccess final
    {
        // The command result itself is not nullable.
        // If the stored value is nullable, that is represented inside ColCmdValue.
        ColCmdValue value;
    };

    struct TableInfoCmdExecSuccess final
    {
        TableName tableName;
        CmdTypeKindValue keyColType;
        CmdTypeKindValue valueColType;
        std::uint64_t rowsCount = 0;
    };

    using SuccessCmdExecResult = std::variant<
        EmptyCmdExecSuccess,
        TransactionOpCmdExecSuccess,
        TransactionCheckCmdExecSuccess,
        AffectedRowsCmdExecSuccess,
        GetCmdExecSuccess,
        TableInfoCmdExecSuccess
    >;

    using CmdExecErr = std::shared_ptr<const Err>;

    using CmdExecResult = std::variant<
        SuccessCmdExecResult,
        CmdExecErr
    >;
}
