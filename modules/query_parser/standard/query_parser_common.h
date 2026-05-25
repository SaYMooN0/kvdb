#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "i_modules.h"

namespace kvdb::modules::query_parser::standard {
    namespace Keyword {
        inline constexpr std::string_view Begin = "Begin";
        inline constexpr std::string_view Commit = "Commit";
        inline constexpr std::string_view Rollback = "Rollback";
        inline constexpr std::string_view AnyTransaction = "AnyTransaction";

        inline constexpr std::string_view CreateTable = "CreateTable";
        inline constexpr std::string_view Erase = "Erase";
        inline constexpr std::string_view EnsureErased = "EnsureErased";
        inline constexpr std::string_view TableInfo = "TableInfo";

        inline constexpr std::string_view In = "In";
        inline constexpr std::string_view Get = "Get";
        inline constexpr std::string_view Set = "Set";
        inline constexpr std::string_view Delete = "Delete";
        inline constexpr std::string_view EnsureDeleted = "EnsureDeleted";
        inline constexpr std::string_view To = "To";

        inline constexpr std::string_view Null = "Null";
        inline constexpr std::string_view True = "True";
        inline constexpr std::string_view False = "False";
    }

    namespace TypeKeyword {
        inline constexpr std::string_view Uuid = "uuid";
        inline constexpr std::string_view CharSeq = "charseq";
        inline constexpr std::string_view Int = "int";
        inline constexpr std::string_view UInt = "uint";
        inline constexpr std::string_view Bool = "bool";
        inline constexpr std::string_view Float = "float";
        inline constexpr std::string_view Nullable = "nullable";
        inline constexpr std::string_view Array = "array";
    }

    struct QueryParserErr final : kvdb::contracts::Err
    {
        std::string codeValue;
        std::string messageValue;

        QueryParserErr(std::string code, std::string message)
            : codeValue(std::move(code)),
              messageValue(std::move(message)) {}

        [[nodiscard]]
        std::string_view module() const noexcept override {
            return "query_parser";
        }

        [[nodiscard]]
        std::string_view code() const noexcept override {
            return codeValue;
        }

        [[nodiscard]]
        std::string_view message() const noexcept override {
            return messageValue;
        }
    };

    struct ParseFailure final
    {
        kvdb::contracts::CmdParseErr err;
    };

    [[noreturn]]
    inline void fail(std::string message, std::string code = "InvalidSyntax") {
        throw ParseFailure{
            std::make_shared<QueryParserErr>(std::move(code), std::move(message))
        };
    }
}
