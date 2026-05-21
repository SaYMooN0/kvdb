#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>
#include <variant>

#include "i_modules.h"
#include "cmds/cmd_dtos.h"
#include "cmds/cmd_parse_results.h"

using namespace kvdb::contracts;

extern "C" IQueryParser* create_query_parser();
extern "C" void destroy_query_parser(IQueryParser* parser);

namespace {
    struct QueryParserDeleter
    {
        void operator()(IQueryParser* parser) const {
            if (parser != nullptr) {
                destroy_query_parser(parser);
            }
        }
    };

    using QueryParserPtr = std::unique_ptr<IQueryParser, QueryParserDeleter>;

    QueryParserPtr makeQueryParserForTests() {
        return QueryParserPtr(create_query_parser());
    }

    const BaseCmdDto& requireParsedCmd(CmdParseResult& result, CmdKind expectedKind) {
        REQUIRE(std::holds_alternative<CmdParseSuccess>(result));

        const auto& cmd = std::get<CmdParseSuccess>(result);
        REQUIRE(cmd != nullptr);
        REQUIRE(cmd->kind() == expectedKind);

        return *cmd;
    }

    void requireParseErr(CmdParseResult& result) {
        REQUIRE(std::holds_alternative<CmdParseErr>(result));

        const auto& err = std::get<CmdParseErr>(result);
        REQUIRE(err != nullptr);
        REQUIRE_FALSE(err->code().empty());
        REQUIRE_FALSE(err->message().empty());
    }

    constexpr const char* CREATE_TABLE_CMD =
        R"(CreateTable "users" 'charseq(16) To 'charseq(128))";

    constexpr const char* SET_CMD =
        R"(In "users" Set "user-1" To "John")";

    constexpr const char* GET_CMD =
        R"(In "users" Get "user-1")";

    constexpr const char* DEL_CMD =
        R"(In "users" Del "user-1")";

    constexpr const char* ERASE_TABLE_CMD =
        R"(EraseTable "users")";

    constexpr const char* TABLE_INFO_CMD =
        R"(TableInfo "users")";

    constexpr const char* BEGIN_CMD =
        R"(Begin)";

    constexpr const char* COMMIT_CMD =
        R"(Commit)";

    constexpr const char* ROLLBACK_CMD =
        R"(Rollback)";

    constexpr const char* ANY_TRANSACTION_CMD =
        R"(AnyTransaction)";

    constexpr const char* INVALID_CMD =
        R"(THIS IS NOT A KVDB COMMAND)";
}

TEST_CASE("query_parser: parse creates CreateTable command") {
    auto parser = makeQueryParserForTests();
    REQUIRE(parser != nullptr);

    auto result = parser->parse(CREATE_TABLE_CMD);

    requireParsedCmd(result, CmdKind::CreateTable);
}

TEST_CASE("query_parser: parse creates Set command") {
    auto parser = makeQueryParserForTests();
    REQUIRE(parser != nullptr);

    auto result = parser->parse(SET_CMD);

    requireParsedCmd(result, CmdKind::Set);
}

TEST_CASE("query_parser: parse creates Get command") {
    auto parser = makeQueryParserForTests();
    REQUIRE(parser != nullptr);

    auto result = parser->parse(GET_CMD);

    requireParsedCmd(result, CmdKind::Get);
}

TEST_CASE("query_parser: parse creates Del command") {
    auto parser = makeQueryParserForTests();
    REQUIRE(parser != nullptr);

    auto result = parser->parse(DEL_CMD);

    requireParsedCmd(result, CmdKind::Del);
}

TEST_CASE("query_parser: parse creates table management commands") {
    auto parser = makeQueryParserForTests();
    REQUIRE(parser != nullptr);

    SECTION("EraseTable") {
        auto result = parser->parse(ERASE_TABLE_CMD);
        requireParsedCmd(result, CmdKind::EraseTable);
    }
}

TEST_CASE("query_parser: parse creates transaction commands") {
    auto parser = makeQueryParserForTests();
    REQUIRE(parser != nullptr);

    SECTION("Begin") {
        auto result = parser->parse(BEGIN_CMD);
        requireParsedCmd(result, CmdKind::Begin);
    }

    SECTION("Commit") {
        auto result = parser->parse(COMMIT_CMD);
        requireParsedCmd(result, CmdKind::Commit);
    }

    SECTION("Rollback") {
        auto result = parser->parse(ROLLBACK_CMD);
        requireParsedCmd(result, CmdKind::Rollback);
    }

    SECTION("AnyTransaction") {
        auto result = parser->parse(ANY_TRANSACTION_CMD);
        requireParsedCmd(result, CmdKind::AnyTransaction);
    }
}

TEST_CASE("query_parser: parse returns error for invalid input") {
    auto parser = makeQueryParserForTests();
    REQUIRE(parser != nullptr);

    SECTION("empty command") {
        auto result = parser->parse("");
        requireParseErr(result);
    }

    SECTION("unknown command") {
        auto result = parser->parse(INVALID_CMD);
        requireParseErr(result);
    }

    SECTION("broken create table command") {
        auto result = parser->parse(R"(CREATE TABLE)");
        requireParseErr(result);
    }

    SECTION("broken set command") {
        auto result = parser->parse(R"(SET users)");
        requireParseErr(result);
    }

    SECTION("broken get command") {
        auto result = parser->parse(R"(GET)");
        requireParseErr(result);
    }
}
