#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>
#include <variant>

#include "i_modules.h"
#include "cmds/cmd_dtos.h"
#include "cmds/cmd_exec_results.h"
#include "cmds/cmd_parse_results.h"
#include "cmds/cmd_values.h"

using namespace kvdb::contracts;

extern "C" IQueryParser* create_query_parser();
extern "C" void destroy_query_parser(IQueryParser* parser);

extern "C" IEngine* create_engine();
extern "C" void destroy_engine(IEngine* engine);

namespace {
    struct QueryParserDeleter
    {
        void operator()(IQueryParser* parser) const {
            if (parser != nullptr) {
                destroy_query_parser(parser);
            }
        }
    };

    struct EngineDeleter
    {
        void operator()(IEngine* engine) const {
            if (engine != nullptr) {
                destroy_engine(engine);
            }
        }
    };

    using QueryParserPtr = std::unique_ptr<IQueryParser, QueryParserDeleter>;
    using EnginePtr = std::unique_ptr<IEngine, EngineDeleter>;

    QueryParserPtr makeQueryParserForTests() {
        return QueryParserPtr(create_query_parser());
    }

    EnginePtr makeEngineForTests() {
        return EnginePtr(create_engine());
    }

    struct EngineTestEnv
    {
        QueryParserPtr parser = makeQueryParserForTests();
        EnginePtr engine = makeEngineForTests();

        EngineTestEnv() {
            REQUIRE(parser != nullptr);
            REQUIRE(engine != nullptr);

            engine->onInstanceStart();
        }

        ~EngineTestEnv() {
            if (engine != nullptr) {
                engine->onInstanceShutdown();
            }
        }
    };

    int nextTableId() {
        static int id = 0;
        return ++id;
    }

    std::string uniqueTableName(const std::string& prefix) {
        return prefix + "_" + std::to_string(nextTableId());
    }

    std::string quoted(const std::string& value) {
        return "\"" + value + "\"";
    }

    std::string createTableCmd(const std::string& tableName) {
        return "CreateTable " + quoted(tableName) + " 'charseq(64) To 'charseq(128)";
    }

    std::string setCmd(
        const std::string& tableName,
        const std::string& key,
        const std::string& value
    ) {
        return "In " + quoted(tableName) + " Set " + quoted(key) + " To " + quoted(value);
    }

    std::string getCmd(
        const std::string& tableName,
        const std::string& key
    ) {
        return "In " + quoted(tableName) + " Get " + quoted(key);
    }

    std::string delCmd(
        const std::string& tableName,
        const std::string& key
    ) {
        return "In " + quoted(tableName) + " Delete " + quoted(key);
    }

    constexpr const char* BEGIN_CMD = "Begin";
    constexpr const char* COMMIT_CMD = "Commit";
    constexpr const char* ROLLBACK_CMD = "Rollback";
    constexpr const char* ANY_TRANSACTION_CMD = "AnyTransaction";

    BaseCmdDto& requireParsedCmd(
        CmdParseResult& result,
        const std::string& rawQuery
    ) {
        if (std::holds_alternative<CmdParseErr>(result)) {
            const auto& err = std::get<CmdParseErr>(result);

            if (err != nullptr) {
                FAIL(
                    "Expected parse success, but got parse error.\n"
                    << "Raw query: " << rawQuery << "\n"
                    << "Error: "
                    << std::string(err->module())
                    << "."
                    << std::string(err->code())
                    << ": "
                    << std::string(err->message())
                );
            }

            FAIL(
                "Expected parse success, but got nullptr parse error.\n"
                << "Raw query: " << rawQuery
            );
        }

        REQUIRE(std::holds_alternative<CmdParseSuccess>(result));

        auto& cmd = std::get<CmdParseSuccess>(result);
        REQUIRE(cmd != nullptr);

        return *cmd;
    }

    CmdExecResult executeRaw(
        IQueryParser& parser,
        IEngine& engine,
        const std::string& rawQuery
    ) {
        INFO("Executing query: " << rawQuery);

        auto parsed = parser.parse(rawQuery);
        BaseCmdDto& cmd = requireParsedCmd(parsed, rawQuery);

        return engine.execute(cmd);
    }

    const SuccessCmdExecResult& requireExecSuccess(
        const CmdExecResult& result,
        const std::string& rawQuery
    ) {
        if (std::holds_alternative<CmdExecErr>(result)) {
            const auto& err = std::get<CmdExecErr>(result);

            if (err != nullptr) {
                FAIL(
                    "Expected exec success, but got exec error.\n"
                    << "Raw query: " << rawQuery << "\n"
                    << "Error: "
                    << std::string(err->module())
                    << "."
                    << std::string(err->code())
                    << ": "
                    << std::string(err->message())
                );
            }

            FAIL(
                "Expected exec success, but got nullptr exec error.\n"
                << "Raw query: " << rawQuery
            );
        }

        REQUIRE(std::holds_alternative<SuccessCmdExecResult>(result));
        return std::get<SuccessCmdExecResult>(result);
    }

    std::shared_ptr<const Err> requireExecErr(
        const CmdExecResult& result,
        const std::string& rawQuery
    ) {
        if (std::holds_alternative<SuccessCmdExecResult>(result)) {
            FAIL(
                "Expected exec error, but got success.\n"
                << "Raw query: " << rawQuery
            );
        }

        REQUIRE(std::holds_alternative<CmdExecErr>(result));

        auto err = std::get<CmdExecErr>(result);
        REQUIRE(err != nullptr);

        return err;
    }

    void executeRawAndRequireSuccess(
        IQueryParser& parser,
        IEngine& engine,
        const std::string& rawQuery
    ) {
        auto result = executeRaw(parser, engine, rawQuery);
        requireExecSuccess(result, rawQuery);
    }
}

TEST_CASE("engine: lifecycle methods can be called", "[engine_tests]") {
    auto engine = makeEngineForTests();

    REQUIRE(engine != nullptr);

    REQUIRE_NOTHROW(engine->onInstanceStart());
    REQUIRE_NOTHROW(engine->onInstanceShutdown());
}

TEST_CASE("engine: creates table successfully", "[engine_tests]") {
    EngineTestEnv env;

    const std::string table = uniqueTableName("eng_create");
    const std::string cmd = createTableCmd(table);

    auto result = executeRaw(*env.parser, *env.engine, cmd);
    const auto& success = requireExecSuccess(result, cmd);

    REQUIRE(std::holds_alternative<EmptyCmdExecSuccess>(success));
}

TEST_CASE("engine: creates two independent tables", "[engine_tests]") {
    EngineTestEnv env;

    const std::string firstTable = uniqueTableName("eng_multi_first");
    const std::string secondTable = uniqueTableName("eng_multi_second");

    const std::string firstCreateCmd = createTableCmd(firstTable);
    const std::string secondCreateCmd = createTableCmd(secondTable);

    auto firstResult = executeRaw(*env.parser, *env.engine, firstCreateCmd);
    const auto& firstSuccess = requireExecSuccess(firstResult, firstCreateCmd);
    REQUIRE(std::holds_alternative<EmptyCmdExecSuccess>(firstSuccess));

    auto secondResult = executeRaw(*env.parser, *env.engine, secondCreateCmd);
    const auto& secondSuccess = requireExecSuccess(secondResult, secondCreateCmd);
    REQUIRE(std::holds_alternative<EmptyCmdExecSuccess>(secondSuccess));
}

TEST_CASE("engine: Set then Get returns value", "[engine_tests]") {
    EngineTestEnv env;

    const std::string table = uniqueTableName("eng_set_get");
    const std::string key = "key_1";
    const std::string value = "John";

    executeRawAndRequireSuccess(*env.parser, *env.engine, createTableCmd(table));
    executeRawAndRequireSuccess(*env.parser, *env.engine, setCmd(table, key, value));

    const std::string getRawCmd = getCmd(table, key);
    auto getResult = executeRaw(*env.parser, *env.engine, getRawCmd);
    const auto& getSuccess = requireExecSuccess(getResult, getRawCmd);

    REQUIRE(std::holds_alternative<GetCmdExecSuccess>(getSuccess));

    const auto& storedValue = std::get<GetCmdExecSuccess>(getSuccess).value;

    REQUIRE(storedValue.kind == ColCmdValueKind::Plain);
    REQUIRE(storedValue.plain.kind == PrimitiveCmdValueKind::CharSeq);
    REQUIRE(storedValue.plain.charSeq.utf8Value != nullptr);

    REQUIRE(std::string(
        storedValue.plain.charSeq.utf8Value,
        storedValue.plain.charSeq.byteLength
    ) == value);
}

TEST_CASE("engine: Set overwrites existing value", "[engine_tests]") {
    EngineTestEnv env;

    const std::string table = uniqueTableName("eng_overwrite");
    const std::string key = "key_1";
    const std::string firstValue = "First";
    const std::string secondValue = "Second";

    executeRawAndRequireSuccess(*env.parser, *env.engine, createTableCmd(table));
    executeRawAndRequireSuccess(*env.parser, *env.engine, setCmd(table, key, firstValue));
    executeRawAndRequireSuccess(*env.parser, *env.engine, setCmd(table, key, secondValue));

    const std::string getRawCmd = getCmd(table, key);
    auto getResult = executeRaw(*env.parser, *env.engine, getRawCmd);
    const auto& getSuccess = requireExecSuccess(getResult, getRawCmd);

    REQUIRE(std::holds_alternative<GetCmdExecSuccess>(getSuccess));

    const auto& storedValue = std::get<GetCmdExecSuccess>(getSuccess).value;

    REQUIRE(storedValue.kind == ColCmdValueKind::Plain);
    REQUIRE(storedValue.plain.kind == PrimitiveCmdValueKind::CharSeq);

    REQUIRE(std::string(
        storedValue.plain.charSeq.utf8Value,
        storedValue.plain.charSeq.byteLength
    ) == secondValue);
}

TEST_CASE("engine: Delete removes existing value", "[engine_tests]") {
    EngineTestEnv env;

    const std::string table = uniqueTableName("eng_del");
    const std::string key = "key_1";
    const std::string value = "John";

    executeRawAndRequireSuccess(*env.parser, *env.engine, createTableCmd(table));
    executeRawAndRequireSuccess(*env.parser, *env.engine, setCmd(table, key, value));

    const std::string delRawCmd = delCmd(table, key);
    auto delResult = executeRaw(*env.parser, *env.engine, delRawCmd);
    const auto& delSuccess = requireExecSuccess(delResult, delRawCmd);

    REQUIRE(std::holds_alternative<AffectedRowsCmdExecSuccess>(delSuccess));
    REQUIRE(std::get<AffectedRowsCmdExecSuccess>(delSuccess).count == 1);

    const std::string getRawCmd = getCmd(table, key);
    auto getAfterDelResult = executeRaw(*env.parser, *env.engine, getRawCmd);

    requireExecErr(getAfterDelResult, getRawCmd);
}

TEST_CASE("engine: Get missing key returns error", "[engine_tests]") {
    EngineTestEnv env;

    const std::string table = uniqueTableName("eng_missing");
    const std::string key = "missing_key";

    executeRawAndRequireSuccess(*env.parser, *env.engine, createTableCmd(table));

    const std::string getRawCmd = getCmd(table, key);
    auto getResult = executeRaw(*env.parser, *env.engine, getRawCmd);

    const auto err = requireExecErr(getResult, getRawCmd);

    REQUIRE_FALSE(err->module().empty());
    REQUIRE_FALSE(err->code().empty());
    REQUIRE_FALSE(err->message().empty());
}

TEST_CASE("engine: Commit preserves transaction changes", "[engine_tests]") {
    EngineTestEnv env;

    const std::string table = uniqueTableName("eng_commit");
    const std::string key = "key_1";
    const std::string value = "Committed";

    executeRawAndRequireSuccess(*env.parser, *env.engine, createTableCmd(table));
    executeRawAndRequireSuccess(*env.parser, *env.engine, BEGIN_CMD);
    executeRawAndRequireSuccess(*env.parser, *env.engine, setCmd(table, key, value));
    executeRawAndRequireSuccess(*env.parser, *env.engine, COMMIT_CMD);

    const std::string getRawCmd = getCmd(table, key);
    auto getResult = executeRaw(*env.parser, *env.engine, getRawCmd);
    const auto& getSuccess = requireExecSuccess(getResult, getRawCmd);

    REQUIRE(std::holds_alternative<GetCmdExecSuccess>(getSuccess));

    const auto& storedValue = std::get<GetCmdExecSuccess>(getSuccess).value;

    REQUIRE(storedValue.kind == ColCmdValueKind::Plain);
    REQUIRE(storedValue.plain.kind == PrimitiveCmdValueKind::CharSeq);

    REQUIRE(std::string(
        storedValue.plain.charSeq.utf8Value,
        storedValue.plain.charSeq.byteLength
    ) == value);
}

TEST_CASE("engine: Rollback cancels transaction changes", "[engine_tests]") {
    EngineTestEnv env;

    const std::string table = uniqueTableName("eng_rollback");
    const std::string key = "key_1";
    const std::string value = "RolledBack";

    executeRawAndRequireSuccess(*env.parser, *env.engine, createTableCmd(table));
    executeRawAndRequireSuccess(*env.parser, *env.engine, BEGIN_CMD);
    executeRawAndRequireSuccess(*env.parser, *env.engine, setCmd(table, key, value));
    executeRawAndRequireSuccess(*env.parser, *env.engine, ROLLBACK_CMD);

    const std::string getRawCmd = getCmd(table, key);
    auto getResult = executeRaw(*env.parser, *env.engine, getRawCmd);

    requireExecErr(getResult, getRawCmd);
}

TEST_CASE("engine: AnyTransaction shows transaction state", "[engine_tests]") {
    EngineTestEnv env;

    {
        auto result = executeRaw(*env.parser, *env.engine, ANY_TRANSACTION_CMD);
        const auto& success = requireExecSuccess(result, ANY_TRANSACTION_CMD);

        REQUIRE(std::holds_alternative<TransactionCheckCmdExecSuccess>(success));
        REQUIRE_FALSE(std::get<TransactionCheckCmdExecSuccess>(success).transactionActive);
    }

    executeRawAndRequireSuccess(*env.parser, *env.engine, BEGIN_CMD);

    {
        auto result = executeRaw(*env.parser, *env.engine, ANY_TRANSACTION_CMD);
        const auto& success = requireExecSuccess(result, ANY_TRANSACTION_CMD);

        REQUIRE(std::holds_alternative<TransactionCheckCmdExecSuccess>(success));
        REQUIRE(std::get<TransactionCheckCmdExecSuccess>(success).transactionActive);
    }

    executeRawAndRequireSuccess(*env.parser, *env.engine, ROLLBACK_CMD);

    {
        auto result = executeRaw(*env.parser, *env.engine, ANY_TRANSACTION_CMD);
        const auto& success = requireExecSuccess(result, ANY_TRANSACTION_CMD);

        REQUIRE(std::holds_alternative<TransactionCheckCmdExecSuccess>(success));
        REQUIRE_FALSE(std::get<TransactionCheckCmdExecSuccess>(success).transactionActive);
    }
}