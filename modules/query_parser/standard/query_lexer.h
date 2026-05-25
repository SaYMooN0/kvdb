#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace kvdb::modules::query_parser::standard {
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

    class Lexer final
    {
    public:
        explicit Lexer(std::string_view input);

        Token next();

    private:
        void skipSpaces();
        [[nodiscard]] bool isNumberStart() const;
        [[nodiscard]] bool isUuidAtCurrentPosition() const;

        Token readUuid();
        Token readIdent();
        Token readNumber();
        Token readString();

    private:
        std::string_view input_;
        std::size_t pos_ = 0;
    };

    [[nodiscard]]
    std::string tokenDescription(const Token& token);
}
