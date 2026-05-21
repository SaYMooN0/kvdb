#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <type_traits>
#include <variant>

#include "err.h"
#include "table_name.h"
#include "cmds/cmd_values.h"
#include "cmds/cmd_dtos.h"
#include "cmds/cmd_exec_results.h"
#include "cmds/cmd_parse_results.h"

using namespace kvdb::contracts;

namespace {
    bool isTableNameCreated(const TableName::Creation& result) {
        return std::holds_alternative<TableName>(result);
    }

    std::shared_ptr<const Err> requireTableNameErr(const TableName::Creation& result) {
        REQUIRE(std::holds_alternative<std::shared_ptr<const Err>>(result));
        auto err = std::get<std::shared_ptr<const Err>>(result);
        REQUIRE(err != nullptr);
        return err;
    }
}

TEST_CASE("contracts: TableName::create accepts valid names") {
    SECTION("latin letters") {
        const auto result = TableName::create("users");
        REQUIRE(isTableNameCreated(result));
        REQUIRE(std::get<TableName>(result).value() == "users");
    }

    SECTION("latin letters and digits") {
        const auto result = TableName::create("users123");
        REQUIRE(isTableNameCreated(result));
    }

    SECTION("underscore") {
        const auto result = TableName::create("user_table");
        REQUIRE(isTableNameCreated(result));
    }

    SECTION("hyphen") {
        const auto result = TableName::create("user-table");
        REQUIRE(isTableNameCreated(result));
    }

    SECTION("russian letters") {
        const auto result = TableName::create("таблица_1");
        REQUIRE(isTableNameCreated(result));
    }
}

TEST_CASE("contracts: TableName::create rejects names with invalid length") {
    SECTION("empty name") {
        const auto err = requireTableNameErr(TableName::create(""));
        REQUIRE(err->module() == "contracts");
        REQUIRE(err->code() == "InvalidTableName");
        REQUIRE(std::string(err->message()).find("length") != std::string::npos);
    }

    SECTION("one character name") {
        const auto err = requireTableNameErr(TableName::create("a"));
        REQUIRE(err->code() == "InvalidTableName");
        REQUIRE(std::string(err->message()).find("between 2 and 127") != std::string::npos);
    }

    SECTION("128 character name") {
        const std::string tooLong(128, 'a');
        const auto err = requireTableNameErr(TableName::create(tooLong));
        REQUIRE(err->code() == "InvalidTableName");
        REQUIRE(std::string(err->message()).find("between 2 and 127") != std::string::npos);
    }
}

TEST_CASE("contracts: TableName::create rejects unsupported symbols and invalid UTF-8") {
    SECTION("space") {
        const auto err = requireTableNameErr(TableName::create("bad name"));
        REQUIRE(err->code() == "InvalidTableName");
        REQUIRE(std::string(err->message()).find("unsupported characters") != std::string::npos);
    }

    SECTION("dot") {
        const auto err = requireTableNameErr(TableName::create("bad.name"));
        REQUIRE(err->code() == "InvalidTableName");
        REQUIRE(std::string(err->message()).find("unsupported characters") != std::string::npos);
    }

    SECTION("dollar sign") {
        const auto err = requireTableNameErr(TableName::create("bad$name"));
        REQUIRE(err->code() == "InvalidTableName");
        REQUIRE(std::string(err->message()).find("unsupported characters") != std::string::npos);
    }

    SECTION("broken UTF-8 sequence") {
        std::string invalidUtf8;
        invalidUtf8.push_back(static_cast<char>(0xC3));

        const auto err = requireTableNameErr(TableName::create(invalidUtf8));
        REQUIRE(err->code() == "InvalidTableName");
        REQUIRE(std::string(err->message()).find("valid UTF-8") != std::string::npos);
    }
}

TEST_CASE("contracts: Err implementations expose stable module, code and message") {
    SECTION("NullableKeyTypeErr") {
        const NullableKeyTypeErr err;
        REQUIRE(err.module() == "contracts");
        REQUIRE(err.code() == "NullableKeyType");
        REQUIRE(err.message() == "Key type cannot be nullable.");
    }

    SECTION("ArrayKeyTypeErr") {
        const ArrayKeyTypeErr err;
        REQUIRE(err.module() == "contracts");
        REQUIRE(err.code() == "ArrayKeyType");
        REQUIRE(err.message() == "Key type cannot be an array.");
    }

    SECTION("NestedNullableTypeErr") {
        const NestedNullableTypeErr err;
        REQUIRE(err.module() == "contracts");
        REQUIRE(err.code() == "NestedNullableType");
        REQUIRE(err.message() == "Nested nullable types are not allowed.");
    }

    SECTION("InvalidCharSeqLengthErr") {
        const InvalidCharSeqLengthErr err(0);
        REQUIRE(err.module() == "contracts");
        REQUIRE(err.code() == "InvalidCharSeqLength");
        REQUIRE(err.message() == "Invalid charseq length: 0.");
    }

    SECTION("InvalidTableNameErr") {
        const InvalidTableNameErr err("bad$name", "Table name contains unsupported characters.");
        REQUIRE(err.module() == "contracts");
        REQUIRE(err.code() == "InvalidTableName");
        REQUIRE(std::string(err.message()).find("bad$name") != std::string::npos);
    }
}

TEST_CASE("contracts: command DTOs without table fields return correct kind") {
    SECTION("BeginCmdDto") {
        const BeginCmdDto cmd;
        REQUIRE(cmd.kind() == CmdKind::Begin);
    }

    SECTION("CommitCmdDto") {
        const CommitCmdDto cmd;
        REQUIRE(cmd.kind() == CmdKind::Commit);
    }

    SECTION("RollbackCmdDto") {
        const RollbackCmdDto cmd;
        REQUIRE(cmd.kind() == CmdKind::Rollback);
    }

    SECTION("AnyTransactionCmdDto") {
        const AnyTransactionCmdDto cmd;
        REQUIRE(cmd.kind() == CmdKind::AnyTransaction);
    }

    SECTION("DTOs with TableName currently need explicit constructors before direct unit construction") {
        REQUIRE_FALSE(std::is_default_constructible_v<CreateTableCmdDto>);
        REQUIRE_FALSE(std::is_default_constructible_v<SetCmdDto>);
        REQUIRE_FALSE(std::is_default_constructible_v<GetCmdDto>);
    }
}

TEST_CASE("contracts: command value DTOs have correct default state") {
    SECTION("PrimitiveCmdValue defaults to Bool false") {
        const PrimitiveCmdValue value;
        REQUIRE(value.kind == PrimitiveCmdValueKind::Bool);
        REQUIRE(value.boolean.value == false);
    }

    SECTION("ColCmdValue defaults to plain primitive value") {
        const ColCmdValue value;
        REQUIRE(value.kind == ColCmdValueKind::Plain);
        REQUIRE(value.plain.kind == PrimitiveCmdValueKind::Bool);
        REQUIRE(value.plain.boolean.value == false);
    }

    SECTION("NullablePrimitiveCmdValue defaults to null") {
        const NullablePrimitiveCmdValue value;
        REQUIRE(value.hasValue == false);
    }

    SECTION("PrimitiveCmdValueArrayView defaults can represent empty array") {
        const PrimitiveCmdValueArrayView view;
        REQUIRE(view.items == nullptr);
        REQUIRE(view.count == 0);
    }

    SECTION("CmdTypeKindValue can describe charseq with size parameter") {
        const CmdTypeKindValue type{CmdTypeKind::CharSeq, 128, nullptr};
        REQUIRE(type.type == CmdTypeKind::CharSeq);
        REQUIRE(type.sizeParam == 128);
        REQUIRE(type.typeParam == nullptr);
    }
}

TEST_CASE("contracts: command result variants keep success and error separately") {
    SECTION("CmdExecResult can store success") {
        CmdExecResult result = SuccessCmdExecResult{EmptyCmdExecSuccess{}};

        REQUIRE(std::holds_alternative<SuccessCmdExecResult>(result));
        const auto& success = std::get<SuccessCmdExecResult>(result);
        REQUIRE(std::holds_alternative<EmptyCmdExecSuccess>(success));
    }

    SECTION("CmdExecResult can store error") {
        CmdExecResult result = std::make_shared<InvalidTableNameErr>(
            "bad$name",
            "Table name contains unsupported characters."
        );

        REQUIRE(std::holds_alternative<CmdExecErr>(result));
        const auto& err = std::get<CmdExecErr>(result);
        REQUIRE(err != nullptr);
        REQUIRE(err->code() == "InvalidTableName");
    }

    SECTION("CmdParseResult can store parse error") {
        CmdParseResult result = std::make_shared<ParserReturnedNullCmdErr>();

        REQUIRE(std::holds_alternative<CmdParseErr>(result));
        const auto& err = std::get<CmdParseErr>(result);
        REQUIRE(err != nullptr);
        REQUIRE(err->code() == "ParserReturnedNullCmd");
    }
}
