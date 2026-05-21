#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <variant>

#include "table_name.h"

using namespace kvdb::contracts;

TEST_CASE("TableName::create accepts valid table names") {
    SECTION("latin name") {
        auto result = TableName::create("users");

        REQUIRE(std::holds_alternative<TableName>(result));
        REQUIRE(std::get<TableName>(result).value() == "users");
    }

    SECTION("name with underscore") {
        auto result = TableName::create("user_table");

        REQUIRE(std::holds_alternative<TableName>(result));
    }

    SECTION("name with hyphen") {
        auto result = TableName::create("user-table");

        REQUIRE(std::holds_alternative<TableName>(result));
    }

    SECTION("name with digits") {
        auto result = TableName::create("users123");

        REQUIRE(std::holds_alternative<TableName>(result));
    }

    SECTION("russian name") {
        auto result = TableName::create("таблица_1");

        REQUIRE(std::holds_alternative<TableName>(result));
    }
}

TEST_CASE("TableName::create rejects invalid table names") {
    SECTION("empty name") {
        auto result = TableName::create("");

        REQUIRE(std::holds_alternative<std::shared_ptr<const Err>>(result));
    }

    SECTION("one character name") {
        auto result = TableName::create("a");

        REQUIRE(std::holds_alternative<std::shared_ptr<const Err>>(result));
    }

    SECTION("name with space") {
        auto result = TableName::create("bad name");

        REQUIRE(std::holds_alternative<std::shared_ptr<const Err>>(result));
    }

    SECTION("name with dollar sign") {
        auto result = TableName::create("bad$name");

        REQUIRE(std::holds_alternative<std::shared_ptr<const Err>>(result));
    }

    SECTION("name with dot") {
        auto result = TableName::create("bad.name");

        REQUIRE(std::holds_alternative<std::shared_ptr<const Err>>(result));
    }
}