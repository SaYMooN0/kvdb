#include "engine_common.h"

#include <memory>
#include <optional>
#include <utility>
#include <variant>
#include "module_export.h"

namespace kvdb::modules::engine::standard {
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

        static Table& ensureMutableTable(const Tables::iterator tableIt) {
            if (!tableIt->second.unique()) {
                tableIt->second = std::make_shared<Table>(*tableIt->second);
            }

            return *tableIt->second;
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

            auto table = std::make_shared<Table>();
            table->keyType = copyType(cmd.keyType);
            table->valueType = copyType(cmd.valueType);

            if (const auto err = validateTableSchema(*table)) {
                return *err;
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

            auto normalizedKey = normalizePrimitive(cmd.keyValue, tableIt->second->keyType);
            if (std::holds_alternative<CmdExecErr>(normalizedKey)) {
                return std::get<CmdExecErr>(std::move(normalizedKey));
            }

            auto normalizedValue = normalizeColValue(cmd.value, tableIt->second->valueType);
            if (std::holds_alternative<CmdExecErr>(normalizedValue)) {
                return std::get<CmdExecErr>(std::move(normalizedValue));
            }

            Table& table = ensureMutableTable(tableIt);
            table.rows[std::get<StoredPrimitive>(std::move(normalizedKey))]
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

            auto normalizedKey = normalizePrimitive(cmd.keyValue, tableIt->second->keyType);
            if (std::holds_alternative<CmdExecErr>(normalizedKey)) {
                return std::get<CmdExecErr>(std::move(normalizedKey));
            }

            const auto rowIt = tableIt->second->rows.find(
                std::get<StoredPrimitive>(std::move(normalizedKey))
            );

            if (rowIt == tableIt->second->rows.end()) {
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

            auto normalizedKey = normalizePrimitive(cmd.keyValue, tableIt->second->keyType);
            if (std::holds_alternative<CmdExecErr>(normalizedKey)) {
                return std::get<CmdExecErr>(std::move(normalizedKey));
            }

            const auto key = std::get<StoredPrimitive>(std::move(normalizedKey));
            if (!tableIt->second->rows.contains(key)) {
                return makeErr(
                    "KeyNotFound",
                    "Key was not found in table '" + tableName + "'."
                );
            }

            Table& table = ensureMutableTable(tableIt);
            table.rows.erase(key);

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

            auto normalizedKey = normalizePrimitive(cmd.keyValue, tableIt->second->keyType);
            if (std::holds_alternative<CmdExecErr>(normalizedKey)) {
                return std::get<CmdExecErr>(std::move(normalizedKey));
            }

            const auto key = std::get<StoredPrimitive>(std::move(normalizedKey));
            if (!tableIt->second->rows.contains(key)) {
                return makeAffectedRowsSuccess(0);
            }

            Table& table = ensureMutableTable(tableIt);
            table.rows.erase(key);

            return makeAffectedRowsSuccess(1);
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

            auto tableNameDto = TableName::create(tableName);
            if (std::holds_alternative<std::shared_ptr<const Err>>(tableNameDto)) {
                return std::get<std::shared_ptr<const Err>>(std::move(tableNameDto));
            }

            const Table& table = *tableIt->second;

            return SuccessCmdExecResult{
                TableInfoCmdExecSuccess{
                    .tableName = std::get<TableName>(std::move(tableNameDto)),
                    .keyColType = toDtoType(table.keyType, resultStorage_),
                    .valueColType = toDtoType(table.valueType, resultStorage_),
                    .rowsCount = static_cast<std::uint64_t>(table.rows.size())
                }
            };
        }

    private:
        Tables committedTables_;
        std::optional<Tables> transactionTables_;
        bool transactionActive_ = false;

        ResultDtoStorage resultStorage_;
    };
}

extern "C" KVDB_MODULE_EXPORT
kvdb::contracts::IEngine* create_engine() {
    return new kvdb::modules::engine::standard::StandardEngine();
}

extern "C" KVDB_MODULE_EXPORT
void destroy_engine(kvdb::contracts::IEngine* ptr) {
    delete ptr;
}
