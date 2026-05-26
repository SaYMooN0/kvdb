#include "query_command_parser.h"

#include <charconv>
#include <cerrno>
#include <cstdlib>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <variant>

#include "query_parser_common.h"
#include "module_export.h"

namespace kvdb::modules::query_parser::standard {
    using namespace kvdb::contracts;

    Parser::Parser(const std::string& rawQuery)
        : lexer_(rawQuery),
          current_(lexer_.next()) {}

    CmdParseResult Parser::parseCommand() {
        try {
            auto cmd = parseRootCommand();
            return CmdParseSuccess{std::move(cmd)};
        }
        catch (const ParseFailure& failure) {
            return failure.err;
        }
    }

    std::unique_ptr<BaseCmdDto> Parser::parseRootCommand() {
        const auto command = readIdentifier("command name");

        if (command == Keyword::Begin) {
            expectEnd();
            return std::make_unique<BeginCmdDto>();
        }

        if (command == Keyword::Commit) {
            expectEnd();
            return std::make_unique<CommitCmdDto>();
        }

        if (command == Keyword::Rollback) {
            expectEnd();
            return std::make_unique<RollbackCmdDto>();
        }

        if (command == Keyword::AnyTransaction) {
            expectEnd();
            return std::make_unique<AnyTransactionCmdDto>();
        }

        if (command == Keyword::CreateTable) {
            return parseCreateTable();
        }

        if (command == Keyword::Erase) {
            return parseEraseTable();
        }

        if (command == Keyword::EnsureErased) {
            return parseEnsureTableErased();
        }

        if (command == Keyword::TableInfo) {
            return parseTableInfo();
        }

        if (command == Keyword::In) {
            return parseTableScopedCommand();
        }

        fail("Unknown command: '" + command + "'.", "UnknownCommand");
    }

    std::unique_ptr<BaseCmdDto> Parser::parseCreateTable() {
        auto tableName = parseTableName();

        auto keyType = parseType();
        validateKeyType(keyType);

        expectKeyword(Keyword::To);

        auto valueType = parseType();
        expectEnd();

        return std::make_unique<CreateTableCmdDto>(
            std::move(tableName),
            keyType,
            valueType,
            std::move(storage_)
        );
    }

    std::unique_ptr<BaseCmdDto> Parser::parseEraseTable() {
        auto tableName = parseTableName();
        expectEnd();
        return std::make_unique<EraseTableCmdDto>(std::move(tableName));
    }

    std::unique_ptr<BaseCmdDto> Parser::parseEnsureTableErased() {
        auto tableName = parseTableName();
        expectEnd();
        return std::make_unique<EnsureTableErasedCmdDto>(std::move(tableName));
    }

    std::unique_ptr<BaseCmdDto> Parser::parseTableInfo() {
        auto tableName = parseTableName();
        expectEnd();
        return std::make_unique<TableInfoCmdDto>(std::move(tableName));
    }

    std::unique_ptr<BaseCmdDto> Parser::parseTableScopedCommand() {
        auto tableName = parseTableName();
        const auto command = readIdentifier("table command");

        if (command == Keyword::Get) {
            auto key = parsePrimitiveValue();
            expectEnd();
            return std::make_unique<GetCmdDto>(
                std::move(tableName),
                key,
                std::move(storage_)
            );
        }

        if (command == Keyword::Delete) {
            auto key = parsePrimitiveValue();
            expectEnd();
            return std::make_unique<DelCmdDto>(
                std::move(tableName),
                key,
                std::move(storage_)
            );
        }

        if (command == Keyword::EnsureDeleted) {
            auto key = parsePrimitiveValue();
            expectEnd();
            return std::make_unique<EnsureDelCmdDto>(
                std::move(tableName),
                key,
                std::move(storage_)
            );
        }

        if (command == Keyword::Set) {
            auto key = parsePrimitiveValue();
            expectKeyword(Keyword::To);
            auto value = parseColumnValue();
            expectEnd();
            return std::make_unique<SetCmdDto>(
                std::move(tableName),
                key,
                value,
                std::move(storage_)
            );
        }

        fail("Unknown table command: '" + command + "'.", "UnknownCommand");
    }

    TableName Parser::parseTableName() {
        if (current_.kind != TokenKind::String) {
            fail("Expected table name as string literal, got " + tokenDescription(current_) + ".");
        }

        auto rawName = std::move(current_.text);
        advance();

        auto created = TableName::create(std::move(rawName));
        if (const auto tableName = std::get_if<TableName>(&created)) {
            return std::move(*tableName);
        }

        throw ParseFailure{std::get<std::shared_ptr<const Err>>(std::move(created))};
    }

    CmdTypeKindValue Parser::parseType() {
        expect(TokenKind::Apostrophe, "type prefix apostrophe");

        const auto typeName = readIdentifier("type name");

        if (typeName == TypeKeyword::Uuid) {
            return {CmdTypeKind::Uuid, 0, nullptr};
        }

        if (typeName == TypeKeyword::Bool) {
            return {CmdTypeKind::Bool, 0, nullptr};
        }

        if (typeName == TypeKeyword::Float) {
            return {CmdTypeKind::Float, 0, nullptr};
        }

        if (typeName == TypeKeyword::CharSeq) {
            const auto length = parseRequiredSizeParam();
            if (length == 0) {
                throw ParseFailure{std::make_shared<InvalidCharSeqLengthErr>(length)};
            }

            return {CmdTypeKind::CharSeq, length, nullptr};
        }

        if (typeName == TypeKeyword::Int) {
            const auto size = parseRequiredSizeParam();
            if (size == 0) {
                throw ParseFailure{std::make_shared<InvalidIntByteCountErr>(size)};
            }

            return {CmdTypeKind::Int, size, nullptr};
        }

        if (typeName == TypeKeyword::UInt) {
            const auto size = parseRequiredSizeParam();
            if (size == 0) {
                throw ParseFailure{std::make_shared<InvalidUIntByteCountErr>(size)};
            }

            return {CmdTypeKind::UInt, size, nullptr};
        }

        if (typeName == TypeKeyword::Nullable) {
            expect(TokenKind::LParen, "'('");
            auto inner = parseType();
            expect(TokenKind::RParen, "')'");

            if (inner.type == CmdTypeKind::Nullable) {
                throw ParseFailure{std::make_shared<NestedNullableTypeErr>()};
            }

            return makeNestedType(CmdTypeKind::Nullable, inner);
        }

        if (typeName == TypeKeyword::Array) {
            expect(TokenKind::LParen, "'('");
            auto inner = parseType();
            expect(TokenKind::RParen, "')'");

            if (inner.type == CmdTypeKind::Array) {
                throw ParseFailure{std::make_shared<NestedArrayTypeErr>()};
            }

            return makeNestedType(CmdTypeKind::Array, inner);
        }

        fail("Unknown type: '" + typeName + "'.", "UnknownType");
    }

    std::uint16_t Parser::parseRequiredSizeParam() {
        if (!accept(TokenKind::LParen)) {
            fail("Expected size parameter in parentheses.");
        }

        if (current_.kind != TokenKind::Number) {
            fail("Expected numeric size parameter, got " + tokenDescription(current_) + ".");
        }

        if (isFloatingNumberText(current_.text)) {
            fail("Size parameter must be an integer.");
        }

        if (!current_.text.empty() && current_.text[0] == '-') {
            fail("Size parameter cannot be negative.");
        }

        unsigned int parsed = 0;
        const auto* begin = current_.text.data();
        const auto* end = begin + current_.text.size();
        const auto [ptr, ec] = std::from_chars(begin, end, parsed);

        if (ec != std::errc{}
            || ptr != end
            || parsed > std::numeric_limits<std::uint16_t>::max()) {
            fail("Invalid size parameter: '" + current_.text + "'.");
        }

        advance();
        expect(TokenKind::RParen, "')'");

        return static_cast<std::uint16_t>(parsed);
    }

    void Parser::validateKeyType(const CmdTypeKindValue& keyType) {
        if (keyType.type == CmdTypeKind::Nullable) {
            throw ParseFailure{std::make_shared<NullableKeyTypeErr>()};
        }

        if (keyType.type == CmdTypeKind::Array) {
            throw ParseFailure{std::make_shared<ArrayKeyTypeErr>()};
        }
    }

    CmdTypeKindValue Parser::makeNestedType(
        const CmdTypeKind kind,
        const CmdTypeKindValue& inner
    ) {
        auto innerNode = std::make_unique<CmdTypeKindValue>(inner);
        auto result = CmdTypeKindValue{kind, 0, innerNode.get()};
        storage_.typeNodes.push_back(std::move(innerNode));
        return result;
    }

    ColCmdValue Parser::parseColumnValue() {
        if (isCurrentKeyword(Keyword::Null)) {
            advance();
            return makeNullColumnValue();
        }

        if (accept(TokenKind::LBracket)) {
            return parseArrayValueAfterOpeningBracket();
        }

        return makePlainColumnValue(parsePrimitiveValue());
    }

    ColCmdValue Parser::parseArrayValueAfterOpeningBracket() {
        std::vector<NullablePrimitiveCmdValue> nullableItems;
        bool hasNullItems = false;

        if (!accept(TokenKind::RBracket)) {
            while (true) {
                if (isCurrentKeyword(Keyword::Null)) {
                    advance();
                    hasNullItems = true;
                    nullableItems.push_back(
                        NullablePrimitiveCmdValue{false, PrimitiveCmdValue{}}
                    );
                }
                else {
                    nullableItems.push_back(
                        NullablePrimitiveCmdValue{true, parsePrimitiveValue()}
                    );
                }

                if (!accept(TokenKind::Comma)) {
                    break;
                }
            }

            expect(TokenKind::RBracket, "']'");
        }

        if (hasNullItems) {
            return makeArrayOfNullableColumnValue(std::move(nullableItems));
        }

        return makeArrayColumnValue(nullableItems);
    }

    ColCmdValue Parser::makeNullColumnValue() {
        ColCmdValue result;
        result.kind = ColCmdValueKind::Nullable;
        result.nullable = NullablePrimitiveCmdValue{false, PrimitiveCmdValue{}};
        return result;
    }

    ColCmdValue Parser::makePlainColumnValue(PrimitiveCmdValue primitive) {
        ColCmdValue result;
        result.kind = ColCmdValueKind::Plain;
        result.plain = primitive;
        return result;
    }

    ColCmdValue Parser::makeArrayColumnValue(
        const std::vector<NullablePrimitiveCmdValue>& nullableItems
    ) {
        std::vector<PrimitiveCmdValue> items;
        items.reserve(nullableItems.size());

        for (const auto& item : nullableItems) {
            items.push_back(item.value);
        }

        auto holder = std::make_shared<std::vector<PrimitiveCmdValue>>(std::move(items));

        ColCmdValue result;
        result.kind = ColCmdValueKind::Array;
        result.array.items = holder->data();
        result.array.count = checkedCount(holder->size());

        storage_.primitiveArrays.push_back(std::move(holder));
        return result;
    }

    ColCmdValue Parser::makeArrayOfNullableColumnValue(
        std::vector<NullablePrimitiveCmdValue> nullableItems
    ) {
        auto holder = std::make_shared<std::vector<NullablePrimitiveCmdValue>>(
            std::move(nullableItems)
        );

        ColCmdValue result;
        result.kind = ColCmdValueKind::ArrayOfNullable;
        result.arrayOfNullable.items = holder->data();
        result.arrayOfNullable.count = checkedCount(holder->size());

        storage_.nullablePrimitiveArrays.push_back(std::move(holder));
        return result;
    }

    std::uint32_t Parser::checkedCount(const std::size_t count) {
        if (count > std::numeric_limits<std::uint32_t>::max()) {
            fail("Array literal is too large.");
        }

        return static_cast<std::uint32_t>(count);
    }

    PrimitiveCmdValue Parser::parsePrimitiveValue() {
        if (current_.kind == TokenKind::String) {
            auto value = std::move(current_.text);
            advance();
            return makeCharSeqValue(std::move(value));
        }

        if (current_.kind == TokenKind::Uuid) {
            auto raw = std::move(current_.text);
            advance();
            return makeUuidValue(raw);
        }

        if (current_.kind == TokenKind::Number) {
            auto value = current_.text;
            advance();
            return makeNumberValue(value);
        }

        if (isCurrentKeyword(Keyword::True) || isCurrentKeyword(Keyword::False)) {
            const bool value = isCurrentKeyword(Keyword::True);
            advance();
            return makeBoolValue(value);
        }

        fail("Expected primitive value, got " + tokenDescription(current_) + ".");
    }

    PrimitiveCmdValue Parser::makeCharSeqValue(std::string value) {
        auto holder = std::make_shared<std::string>(std::move(value));
        if (holder->size() > std::numeric_limits<std::uint32_t>::max()) {
            fail("String literal is too large.");
        }

        PrimitiveCmdValue result;
        result.kind = PrimitiveCmdValueKind::CharSeq;
        result.charSeq = CharSeqCmdValue{
            holder->c_str(),
            static_cast<std::uint32_t>(holder->size())
        };

        storage_.strings.push_back(std::move(holder));
        return result;
    }

    PrimitiveCmdValue Parser::makeBoolValue(const bool value) {
        PrimitiveCmdValue result;
        result.kind = PrimitiveCmdValueKind::Bool;
        result.boolean = BoolCmdValue{value};
        return result;
    }

    bool Parser::isFloatingNumberText(const std::string& raw) {
        return raw.find('.') != std::string::npos
            || raw.find('e') != std::string::npos
            || raw.find('E') != std::string::npos;
    }

    PrimitiveCmdValue Parser::makeNumberValue(const std::string& raw) {
        if (isFloatingNumberText(raw)) {
            char* end = nullptr;
            errno = 0;
            const double parsed = std::strtod(raw.c_str(), &end);

            if (errno == ERANGE || end == nullptr || *end != '\0') {
                fail("Invalid float literal: '" + raw + "'.");
            }

            PrimitiveCmdValue result;
            result.kind = PrimitiveCmdValueKind::Float;
            result.floating = FloatCmdValue{parsed};
            return result;
        }

        PrimitiveCmdValue result;
        result.kind = PrimitiveCmdValueKind::Number;
        result.number = NumberCmdValue{};

        if (!raw.empty() && raw[0] == '-') {
            long long parsed = 0;
            const auto* begin = raw.data();
            const auto* end = begin + raw.size();
            const auto [ptr, ec] = std::from_chars(begin, end, parsed);

            if (ec != std::errc{} || ptr != end) {
                fail("Invalid integer literal: '" + raw + "'.");
            }

            result.number.isSigned = true;
            result.number.byteLength = 8;

            const auto rawBytes = static_cast<std::uint64_t>(parsed);
            for (std::size_t i = 0; i < 8; ++i) {
                result.number.bytes[i] = static_cast<std::uint8_t>((rawBytes >> (i * 8)) & 0xFFu);
            }

            return result;
        }

        unsigned long long parsed = 0;
        const auto* begin = raw.data();
        const auto* end = begin + raw.size();
        const auto [ptr, ec] = std::from_chars(begin, end, parsed);

        if (ec != std::errc{} || ptr != end) {
            fail("Invalid integer literal: '" + raw + "'.");
        }

        result.number.isSigned = false;
        result.number.byteLength = 8;

        for (std::size_t i = 0; i < 8; ++i) {
            result.number.bytes[i] = static_cast<std::uint8_t>((parsed >> (i * 8)) & 0xFFu);
        }

        return result;
    }

    int Parser::hexValue(const char ch) {
        if (ch >= '0' && ch <= '9') return ch - '0';
        if (ch >= 'a' && ch <= 'f') return 10 + (ch - 'a');
        if (ch >= 'A' && ch <= 'F') return 10 + (ch - 'A');
        return -1;
    }

    PrimitiveCmdValue Parser::makeUuidValue(const std::string& raw) {
        std::string compact;
        compact.reserve(32);

        for (const char ch : raw) {
            if (ch != '-') {
                compact.push_back(ch);
            }
        }

        if (compact.size() != 32) {
            fail("Invalid UUID literal: '" + raw + "'.");
        }

        PrimitiveCmdValue result;
        result.kind = PrimitiveCmdValueKind::Uuid;
        result.uuid = UuidCmdValue{};

        for (std::size_t i = 0; i < 16; ++i) {
            const int hi = hexValue(compact[i * 2]);
            const int lo = hexValue(compact[i * 2 + 1]);

            if (hi < 0 || lo < 0) {
                fail("Invalid UUID literal: '" + raw + "'.");
            }

            result.uuid.bytes[i] = static_cast<std::uint8_t>((hi << 4) | lo);
        }

        return result;
    }

    std::string Parser::readIdentifier(std::string_view expectedDescription) {
        if (current_.kind != TokenKind::Ident) {
            fail("Expected " + std::string(expectedDescription) + ", got " + tokenDescription(current_) + ".");
        }

        auto result = std::move(current_.text);
        advance();
        return result;
    }

    void Parser::expectKeyword(std::string_view expected) {
        if (current_.kind != TokenKind::Ident) {
            fail("Expected '" + std::string(expected) + "', got " + tokenDescription(current_) + ".");
        }

        if (current_.text != expected) {
            fail("Expected '" + std::string(expected) + "', got '" + current_.text + "'.");
        }

        advance();
    }

    bool Parser::isCurrentKeyword(std::string_view value) const {
        return current_.kind == TokenKind::Ident && current_.text == value;
    }

    bool Parser::accept(const TokenKind kind) {
        if (current_.kind != kind) {
            return false;
        }

        advance();
        return true;
    }

    void Parser::expect(const TokenKind kind, std::string_view description) {
        if (!accept(kind)) {
            fail("Expected " + std::string(description) + ", got " + tokenDescription(current_) + ".");
        }
    }

    void Parser::expectEnd() {
        if (current_.kind != TokenKind::End) {
            fail("Unexpected token after command: " + tokenDescription(current_) + ".");
        }
    }

    void Parser::advance() {
        current_ = lexer_.next();
    }

    class StandardQueryParser final : public kvdb::contracts::IQueryParser
    {
    public:
        kvdb::contracts::CmdParseResult parse(const std::string& rawQuery) override {
            return Parser(rawQuery).parseCommand();
        }
    };
}

extern "C" KVDB_MODULE_EXPORT
kvdb::contracts::IQueryParser* create_query_parser() {
    return new kvdb::modules::query_parser::standard::StandardQueryParser();
}

extern "C" KVDB_MODULE_EXPORT
void destroy_query_parser(kvdb::contracts::IQueryParser* ptr) {
    delete ptr;
}
