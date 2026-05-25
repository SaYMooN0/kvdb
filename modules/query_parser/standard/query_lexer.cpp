#include "query_lexer.h"

#include <cctype>
#include <utility>

#include "query_parser_common.h"

namespace kvdb::modules::query_parser::standard {
    namespace {
        bool isIdentStart(const unsigned char ch) {
            return std::isalpha(ch) || ch == '_';
        }

        bool isIdentPart(const unsigned char ch) {
            return std::isalnum(ch) || ch == '_';
        }

        bool isHexDigit(const char ch) {
            return (ch >= '0' && ch <= '9')
                || (ch >= 'a' && ch <= 'f')
                || (ch >= 'A' && ch <= 'F');
        }
    }

    Lexer::Lexer(std::string_view input)
        : input_(input) {}

    Token Lexer::next() {
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

        fail("Unexpected character at position " + std::to_string(pos_) + ".");
    }

    void Lexer::skipSpaces() {
        while (pos_ < input_.size()
            && std::isspace(static_cast<unsigned char>(input_[pos_]))) {
            ++pos_;
        }
    }

    bool Lexer::isNumberStart() const {
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

    bool Lexer::isUuidAtCurrentPosition() const {
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

    Token Lexer::readUuid() {
        constexpr std::size_t uuidLen = 36;
        const auto start = pos_;
        pos_ += uuidLen;
        return {TokenKind::Uuid, std::string(input_.substr(start, uuidLen)), start};
    }

    Token Lexer::readIdent() {
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

    Token Lexer::readNumber() {
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

    Token Lexer::readString() {
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
}
