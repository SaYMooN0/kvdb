#include <algorithm>
#include <array>
#include <charconv>
#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "i_modules.h"

namespace kvdb::modules::query_parser::standard {
    namespace {
        using namespace kvdb::contracts;

        std::string toLower(std::string value) {
            std::transform(
                value.begin(),
                value.end(),
                value.begin(),
                [](unsigned char ch) {
                    return static_cast<char>(std::tolower(ch));
                }
            );

            return value;
        }

        struct QueryParserErr final : Err
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
            CmdParseErr err;
        };

        [[noreturn]]
        void fail(std::string message, std::string code = "InvalidSyntax") {
            throw ParseFailure{
                std::make_shared<QueryParserErr>(std::move(code), std::move(message))
            };
        }

        enum class TokenKind {
            Ident,
            String,
            Number,
            Uuid,
            Apostrophe,
            LParen,
            RParen,
            LBracket,
            RBracket,
            Comma,
            End
        };

        struct Token final
        {
            TokenKind kind = TokenKind::End;
            std::string text;
            std::size_t position = 0;
        };

        bool isIdentStart(const unsigned char ch) {
            return std::isalpha(ch) || ch == '_';
        }

        bool isIdentPart(const unsigned char ch) {
            return std::isalnum(ch) || ch == '_' || ch == '?';
        }

        bool isHexDigit(const char ch) {
            return (ch >= '0' && ch <= '9')
                || (ch >= 'a' && ch <= 'f')
                || (ch >= 'A' && ch <= 'F');
        }

        class Lexer final
        {
        public:
            explicit Lexer(std::string_view input)
                : input_(input) {}

            Token next() {
                skipSpaces();

                if (pos_ >= input_.size()) {
                    return {TokenKind::End, {}, pos_};
                }

                const auto start = pos_;
                const auto ch = static_cast<unsigned char>(input_[pos_]);

                if (ch == '\'') { ++pos_; return {TokenKind::Apostrophe, "'", start}; }
                if (ch == '(') { ++pos_; return {TokenKind::LParen, "(", start}; }
                if (ch == ')') { ++pos_; return {TokenKind::RParen, ")", start}; }
                if (ch == '[') { ++pos_; return {TokenKind::LBracket, "[", start}; }
                if (ch == ']') { ++pos_; return {TokenKind::RBracket, "]", start}; }
                if (ch == ',') { ++pos_; return {TokenKind::Comma, ",", start}; }

                if (ch == '"') {
                    return readString();
                }

                if (isUuidAtCurrentPosition()) {
                    return readUuid();
                }

                if (isNumberStart()) {
                    return readNumber();
                }

                if (isIdentStart(ch)) {
                    return readIdent();
                }

                fail("111Unexpected character at position " + std::to_string(pos_) + ".");
            }

        private:
            void skipSpaces() {
                while (pos_ < input_.size()
                    && std::isspace(static_cast<unsigned char>(input_[pos_]))) {
                    ++pos_;
                }
            }

            bool isNumberStart() const {
                const auto ch = input_[pos_];

                if (std::isdigit(static_cast<unsigned char>(ch))) {
                    return true;
                }

                if ((ch == '-' || ch == '+') && pos_ + 1 < input_.size()) {
                    const auto next = input_[pos_ + 1];
                    return std::isdigit(static_cast<unsigned char>(next))
                        || (next == '.'
                            && pos_ + 2 < input_.size()
                            && std::isdigit(static_cast<unsigned char>(input_[pos_ + 2])));
                }

                return false;
            }

            bool isUuidAtCurrentPosition() const {
                constexpr std::size_t uuidLen = 36;
                if (pos_ + uuidLen > input_.size()) {
                    return false;
                }

                for (std::size_t i = 0; i < uuidLen; ++i) {
                    const char ch = input_[pos_ + i];
                    const bool shouldBeDash = i == 8 || i == 13 || i == 18 || i == 23;

                    if (shouldBeDash) {
                        if (ch != '-') {
                            return false;
                        }
                    }
                    else if (!isHexDigit(ch)) {
                        return false;
                    }
                }

                if (pos_ + uuidLen < input_.size()) {
                    const auto next = static_cast<unsigned char>(input_[pos_ + uuidLen]);
                    if (std::isalnum(next) || next == '_' || next == '-') {
                        return false;
                    }
                }

                return true;
            }

            Token readUuid() {
                constexpr std::size_t uuidLen = 36;
                const auto start = pos_;
                pos_ += uuidLen;
                return {TokenKind::Uuid, std::string(input_.substr(start, uuidLen)), start};
            }

            Token readIdent() {
                const auto start = pos_;
                ++pos_;

                while (pos_ < input_.size()
                    && isIdentPart(static_cast<unsigned char>(input_[pos_]))) {
                    ++pos_;
                }

                return {
                    TokenKind::Ident,
                    std::string(input_.substr(start, pos_ - start)),
                    start
                };
            }

            Token readNumber() {
                const auto start = pos_;

                if (input_[pos_] == '-' || input_[pos_] == '+') {
                    ++pos_;
                }

                while (pos_ < input_.size()
                    && std::isdigit(static_cast<unsigned char>(input_[pos_]))) {
                    ++pos_;
                }

                if (pos_ < input_.size() && input_[pos_] == '.') {
                    ++pos_;

                    if (pos_ >= input_.size()
                        || !std::isdigit(static_cast<unsigned char>(input_[pos_]))) {
                        fail("Invalid number literal at position " + std::to_string(start) + ".");
                    }

                    while (pos_ < input_.size()
                        && std::isdigit(static_cast<unsigned char>(input_[pos_]))) {
                        ++pos_;
                    }
                }

                if (pos_ < input_.size() && (input_[pos_] == 'e' || input_[pos_] == 'E')) {
                    ++pos_;

                    if (pos_ < input_.size() && (input_[pos_] == '-' || input_[pos_] == '+')) {
                        ++pos_;
                    }

                    if (pos_ >= input_.size()
                        || !std::isdigit(static_cast<unsigned char>(input_[pos_]))) {
                        fail("Invalid number literal at position " + std::to_string(start) + ".");
                    }

                    while (pos_ < input_.size()
                        && std::isdigit(static_cast<unsigned char>(input_[pos_]))) {
                        ++pos_;
                    }
                }

                return {
                    TokenKind::Number,
                    std::string(input_.substr(start, pos_ - start)),
                    start
                };
            }

            Token readString() {
                const auto start = pos_;
                ++pos_;

                std::string result;
                while (pos_ < input_.size()) {
                    const char ch = input_[pos_++];

                    if (ch == '"') {
                        return {TokenKind::String, std::move(result), start};
                    }

                    if (ch != '\\') {
                        result.push_back(ch);
                        continue;
                    }

                    if (pos_ >= input_.size()) {
                        fail("Unfinished escape sequence in string literal.");
                    }

                    const char escaped = input_[pos_++];
                    switch (escaped) {
                        case '"': result.push_back('"'); break;
                        case '\\': result.push_back('\\'); break;
                        case 'n': result.push_back('\n'); break;
                        case 'r': result.push_back('\r'); break;
                        case 't': result.push_back('\t'); break;
                        default:
                            fail(std::string("Unsupported escape sequence: \\") + escaped + ".");
                    }
                }

                fail("Unclosed string literal at position " + std::to_string(start) + ".");
            }

        private:
            std::string_view input_;
            std::size_t pos_ = 0;
        };

        std::string tokenDescription(const Token& token) {
            switch (token.kind) {
                case TokenKind::Ident: return "identifier '" + token.text + "'";
                case TokenKind::String: return "string literal";
                case TokenKind::Number: return "number '" + token.text + "'";
                case TokenKind::Uuid: return "uuid literal '" + token.text + "'";
                case TokenKind::Apostrophe: return "apostrophe";
                case TokenKind::LParen: return "'('";
                case TokenKind::RParen: return "')'";
                case TokenKind::LBracket: return "'['";
                case TokenKind::RBracket: return "']'";
                case TokenKind::Comma: return "','";
                case TokenKind::End: return "end of query";
            }

            return "unknown token";
        }

        class Parser final
        {
        public:
            explicit Parser(const std::string& rawQuery)
                : lexer_(rawQuery),
                  current_(lexer_.next()) {}

            CmdParseResult parseCommand() {
                try {
                    auto cmd = parseRootCommand();
                    return CmdParseSuccess{std::move(cmd)};
                }
                catch (const ParseFailure& failure) {
                    return failure.err;
                }
            }

        private:
            std::unique_ptr<BaseCmdDto> parseRootCommand() {
                const auto command = expectIdentLower("command name");

                if (command == "begin") {
                    expectEnd();
                    return std::make_unique<BeginCmdDto>();
                }

                if (command == "commit") {
                    expectEnd();
                    return std::make_unique<CommitCmdDto>();
                }

                if (command == "rollback") {
                    expectEnd();
                    return std::make_unique<RollbackCmdDto>();
                }

                if (command == "anytransaction"
                    || command == "any_transaction"
                    || command == "transaction?") {
                    expectEnd();
                    return std::make_unique<AnyTransactionCmdDto>();
                }

                if (command == "create" || command == "createtable") {
                    return parseCreateTable();
                }

                if (command == "erase" || command == "erasetable") {
                    return parseEraseTable();
                }

                if (command == "ensuretableerased"
                    || command == "ensureerase"
                    || command == "ensureerased") {
                    return parseEnsureTableErased();
                }

                if (command == "in") {
                    return parseInCommand();
                }

                fail("Unknown command: '" + command + "'.", "UnknownCommand");
            }

            std::unique_ptr<BaseCmdDto> parseCreateTable() {
                auto tableName = parseTableName();

                auto keyType = parseType();
                validateKeyType(keyType);

                expectIdent("to");

                auto valueType = parseType();
                expectEnd();

                return std::make_unique<CreateTableCmdDto>(
                    std::move(tableName),
                    keyType,
                    valueType,
                    std::move(storage_)
                );
            }

            std::unique_ptr<BaseCmdDto> parseEraseTable() {
                auto tableName = parseTableName();
                expectEnd();
                return std::make_unique<EraseTableCmdDto>(std::move(tableName));
            }

            std::unique_ptr<BaseCmdDto> parseEnsureTableErased() {
                auto tableName = parseTableName();
                expectEnd();
                return std::make_unique<EnsureTableErasedCmdDto>(std::move(tableName));
            }

            std::unique_ptr<BaseCmdDto> parseInCommand() {
                auto tableName = parseTableName();
                const auto command = expectIdentLower("table command");

                if (command == "get") {
                    auto key = parsePrimitiveValue();
                    expectEnd();
                    return std::make_unique<GetCmdDto>(
                        std::move(tableName),
                        key,
                        std::move(storage_)
                    );
                }

                if (command == "del") {
                    auto key = parsePrimitiveValue();
                    expectEnd();
                    return std::make_unique<DelCmdDto>(
                        std::move(tableName),
                        key,
                        std::move(storage_)
                    );
                }

                if (command == "ensuredel") {
                    auto key = parsePrimitiveValue();
                    expectEnd();
                    return std::make_unique<EnsureDelCmdDto>(
                        std::move(tableName),
                        key,
                        std::move(storage_)
                    );
                }

                if (command == "set") {
                    auto key = parsePrimitiveValue();
                    expectIdent("to");
                    auto value = parseColValue();
                    expectEnd();
                    return std::make_unique<SetCmdDto>(
                        std::move(tableName),
                        key,
                        value,
                        std::move(storage_)
                    );
                }

                fail("Unknown command after table name: '" + command + "'.", "UnknownCommand");
            }

            TableName parseTableName() {
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

            CmdTypeKindValue parseType() {
                expect(TokenKind::Apostrophe, "type prefix apostrophe");

                const auto typeName = expectIdentLower("type name");

                if (typeName == "uuid") {
                    return {CmdTypeKind::Uuid, 0, nullptr};
                }

                if (typeName == "bool" || typeName == "boolean") {
                    return {CmdTypeKind::Bool, 0, nullptr};
                }

                if (typeName == "float") {
                    return {CmdTypeKind::Float, 0, nullptr};
                }

                if (typeName == "charseq") {
                    const auto length = parseRequiredSizeParam();
                    if (length == 0) {
                        throw ParseFailure{std::make_shared<InvalidCharSeqLengthErr>(length)};
                    }

                    return {CmdTypeKind::CharSeq, length, nullptr};
                }

                if (typeName == "int") {
                    const auto size = parseRequiredSizeParam();
                    if (size == 0) {
                        throw ParseFailure{std::make_shared<InvalidIntByteCountErr>(size)};
                    }

                    return {CmdTypeKind::Int, size, nullptr};
                }

                if (typeName == "uint") {
                    const auto size = parseRequiredSizeParam();
                    if (size == 0) {
                        throw ParseFailure{std::make_shared<InvalidUIntByteCountErr>(size)};
                    }

                    return {CmdTypeKind::UInt, size, nullptr};
                }

                if (typeName == "nullable") {
                    expect(TokenKind::LParen, "'('");
                    auto inner = parseType();
                    expect(TokenKind::RParen, "')'");

                    if (inner.type == CmdTypeKind::Nullable) {
                        throw ParseFailure{std::make_shared<NestedNullableTypeErr>()};
                    }

                    return makeNestedType(CmdTypeKind::Nullable, inner);
                }

                if (typeName == "array") {
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

            std::uint16_t parseRequiredSizeParam() {
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

            void validateKeyType(const CmdTypeKindValue& keyType) {
                if (keyType.type == CmdTypeKind::Nullable) {
                    throw ParseFailure{std::make_shared<NullableKeyTypeErr>()};
                }

                if (keyType.type == CmdTypeKind::Array) {
                    throw ParseFailure{std::make_shared<ArrayKeyTypeErr>()};
                }
            }

            CmdTypeKindValue makeNestedType(
                const CmdTypeKind kind,
                const CmdTypeKindValue& inner
            ) {
                auto innerNode = std::make_unique<CmdTypeKindValue>(inner);
                auto result = CmdTypeKindValue{kind, 0, innerNode.get()};
                storage_.typeNodes.push_back(std::move(innerNode));
                return result;
            }

            ColCmdValue parseColValue() {
                if (isCurrentIdent("null")) {
                    advance();

                    ColCmdValue result;
                    result.kind = ColCmdValueKind::Nullable;
                    result.nullable = NullablePrimitiveCmdValue{false, PrimitiveCmdValue{}};
                    return result;
                }

                if (accept(TokenKind::LBracket)) {
                    return parseArrayValueAfterOpeningBracket();
                }

                auto primitive = parsePrimitiveValue();

                ColCmdValue result;
                result.kind = ColCmdValueKind::Plain;
                result.plain = primitive;
                return result;
            }

            ColCmdValue parseArrayValueAfterOpeningBracket() {
                std::vector<NullablePrimitiveCmdValue> nullableItems;
                bool hasNullItems = false;

                if (!accept(TokenKind::RBracket)) {
                    while (true) {
                        if (isCurrentIdent("null")) {
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

                ColCmdValue result;

                if (hasNullItems) {
                    auto holder = std::make_shared<std::vector<NullablePrimitiveCmdValue>>(
                        std::move(nullableItems)
                    );

                    result.kind = ColCmdValueKind::ArrayOfNullable;
                    result.arrayOfNullable.items = holder->data();
                    result.arrayOfNullable.count = checkedCount(holder->size());
                    storage_.nullablePrimitiveArrays.push_back(std::move(holder));
                    return result;
                }

                std::vector<PrimitiveCmdValue> items;
                items.reserve(nullableItems.size());

                for (const auto& item : nullableItems) {
                    items.push_back(item.value);
                }

                auto holder = std::make_shared<std::vector<PrimitiveCmdValue>>(std::move(items));

                result.kind = ColCmdValueKind::Array;
                result.array.items = holder->data();
                result.array.count = checkedCount(holder->size());
                storage_.primitiveArrays.push_back(std::move(holder));
                return result;
            }

            static std::uint32_t checkedCount(const std::size_t count) {
                if (count > std::numeric_limits<std::uint32_t>::max()) {
                    fail("Array literal is too large.");
                }

                return static_cast<std::uint32_t>(count);
            }

            PrimitiveCmdValue parsePrimitiveValue() {
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

                if (isCurrentIdent("true") || isCurrentIdent("false")) {
                    const bool value = isCurrentIdent("true");
                    advance();

                    PrimitiveCmdValue result;
                    result.kind = PrimitiveCmdValueKind::Bool;
                    result.boolean = BoolCmdValue{value};
                    return result;
                }

                fail("Expected primitive value, got " + tokenDescription(current_) + ".");
            }

            PrimitiveCmdValue makeCharSeqValue(std::string value) {
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

            static bool isFloatingNumberText(const std::string& raw) {
                return raw.find('.') != std::string::npos
                    || raw.find('e') != std::string::npos
                    || raw.find('E') != std::string::npos;
            }

            static PrimitiveCmdValue makeNumberValue(const std::string& raw) {
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

            static int hexValue(const char ch) {
                if (ch >= '0' && ch <= '9') return ch - '0';
                if (ch >= 'a' && ch <= 'f') return 10 + (ch - 'a');
                if (ch >= 'A' && ch <= 'F') return 10 + (ch - 'A');
                return -1;
            }

            static PrimitiveCmdValue makeUuidValue(const std::string& raw) {
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

            std::string expectIdentLower(std::string_view expectedDescription) {
                if (current_.kind != TokenKind::Ident) {
                    fail("Expected " + std::string(expectedDescription) + ", got " + tokenDescription(current_) + ".");
                }

                auto result = toLower(std::move(current_.text));
                advance();
                return result;
            }

            void expectIdent(std::string_view expected) {
                const auto received = expectIdentLower(std::string(expected));

                if (received != expected) {
                    fail("Expected '" + std::string(expected) + "', got '" + received + "'.");
                }
            }

            bool isCurrentIdent(std::string_view value) const {
                return current_.kind == TokenKind::Ident && toLower(current_.text) == value;
            }

            bool accept(const TokenKind kind) {
                if (current_.kind != kind) {
                    return false;
                }

                advance();
                return true;
            }

            void expect(const TokenKind kind, std::string_view description) {
                if (!accept(kind)) {
                    fail("Expected " + std::string(description) + ", got " + tokenDescription(current_) + ".");
                }
            }

            void expectEnd() {
                if (current_.kind != TokenKind::End) {
                    fail("Unexpected token after command: " + tokenDescription(current_) + ".");
                }
            }

            void advance() {
                current_ = lexer_.next();
            }

        private:
            Lexer lexer_;
            Token current_;
            CmdDtoStorage storage_;
        };
    }

    class StandardQueryParser final : public kvdb::contracts::IQueryParser
    {
    public:
        kvdb::contracts::CmdParseResult parse(const std::string& rawQuery) override {
            return Parser(rawQuery).parseCommand();
        }
    };
}

extern "C" __declspec(dllexport)
kvdb::contracts::IQueryParser* create_query_parser() {
    return new kvdb::modules::query_parser::standard::StandardQueryParser();
}

extern "C" __declspec(dllexport)
void destroy_query_parser(kvdb::contracts::IQueryParser* ptr) {
    delete ptr;
}