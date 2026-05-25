#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "i_modules.h"
#include "query_lexer.h"

namespace kvdb::modules::query_parser::standard {
    class Parser final
    {
    public:
        explicit Parser(const std::string& rawQuery);

        [[nodiscard]]
        contracts::CmdParseResult parseCommand();

    private:
        [[nodiscard]] std::unique_ptr<contracts::BaseCmdDto> parseRootCommand();
        [[nodiscard]] std::unique_ptr<contracts::BaseCmdDto> parseCreateTable();
        [[nodiscard]] std::unique_ptr<contracts::BaseCmdDto> parseEraseTable();
        [[nodiscard]] std::unique_ptr<contracts::BaseCmdDto> parseEnsureTableErased();
        [[nodiscard]] std::unique_ptr<contracts::BaseCmdDto> parseTableInfo();
        [[nodiscard]] std::unique_ptr<contracts::BaseCmdDto> parseTableScopedCommand();

        [[nodiscard]] contracts::TableName parseTableName();
        [[nodiscard]] contracts::CmdTypeKindValue parseType();
        [[nodiscard]] std::uint16_t parseRequiredSizeParam();

        static void validateKeyType(const contracts::CmdTypeKindValue& keyType);
        [[nodiscard]] contracts::CmdTypeKindValue makeNestedType(
            contracts::CmdTypeKind kind,
            const contracts::CmdTypeKindValue& inner
        );

        [[nodiscard]] contracts::ColCmdValue parseColumnValue();
        [[nodiscard]] contracts::ColCmdValue parseArrayValueAfterOpeningBracket();
        [[nodiscard]] static contracts::ColCmdValue makeNullColumnValue();
        [[nodiscard]] static contracts::ColCmdValue makePlainColumnValue(
            contracts::PrimitiveCmdValue primitive
        );
        [[nodiscard]] contracts::ColCmdValue makeArrayColumnValue(
            const std::vector<contracts::NullablePrimitiveCmdValue>& nullableItems
        );
        [[nodiscard]] contracts::ColCmdValue makeArrayOfNullableColumnValue(
            std::vector<contracts::NullablePrimitiveCmdValue> nullableItems
        );
        [[nodiscard]] static std::uint32_t checkedCount(std::size_t count);

        [[nodiscard]] contracts::PrimitiveCmdValue parsePrimitiveValue();
        [[nodiscard]] contracts::PrimitiveCmdValue makeCharSeqValue(std::string value);
        [[nodiscard]] static contracts::PrimitiveCmdValue makeBoolValue(bool value);
        [[nodiscard]] static bool isFloatingNumberText(const std::string& raw);
        [[nodiscard]] static contracts::PrimitiveCmdValue makeNumberValue(const std::string& raw);
        [[nodiscard]] static int hexValue(char ch);
        [[nodiscard]] static contracts::PrimitiveCmdValue makeUuidValue(const std::string& raw);

        [[nodiscard]] std::string readIdentifier(std::string_view expectedDescription);
        void expectKeyword(std::string_view expected);
        [[nodiscard]] bool isCurrentKeyword(std::string_view value) const;

        [[nodiscard]] bool accept(TokenKind kind);
        void expect(TokenKind kind, std::string_view description);
        void expectEnd();
        void advance();

    private:
        Lexer lexer_;
        Token current_;
        contracts::CmdDtoStorage storage_;
    };
}
