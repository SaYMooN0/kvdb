#include "table_name.h"

#include <cstdint>
#include <string_view>
#include <vector>

#include "kvdb_exception.h"

namespace kvdb::contracts {
    namespace {
        bool tryDecodeUtf8(std::string_view input, std::vector<char32_t>& out) {
            out.clear();
            out.reserve(input.size());

            std::size_t i = 0;
            while (i < input.size()) {
                const auto b0 = static_cast<unsigned char>(input[i]);

                if ((b0 & 0b1000'0000) == 0) {
                    out.push_back(static_cast<char32_t>(b0));
                    ++i;
                    continue;
                }

                int length = 0;
                char32_t codePoint = 0;
                char32_t minValue = 0;

                if ((b0 & 0b1110'0000) == 0b1100'0000) {
                    length = 2;
                    codePoint = b0 & 0b0001'1111;
                    minValue = 0x80;
                }
                else if ((b0 & 0b1111'0000) == 0b1110'0000) {
                    length = 3;
                    codePoint = b0 & 0b0000'1111;
                    minValue = 0x800;
                }
                else if ((b0 & 0b1111'1000) == 0b1111'0000) {
                    length = 4;
                    codePoint = b0 & 0b0000'0111;
                    minValue = 0x10000;
                }
                else {
                    return false;
                }

                if (i + static_cast<std::size_t>(length) > input.size()) {
                    return false;
                }

                for (int j = 1; j < length; ++j) {
                    const auto bx = static_cast<unsigned char>(input[i + j]);
                    if ((bx & 0b1100'0000) != 0b1000'0000) {
                        return false;
                    }

                    codePoint = (codePoint << 6) | (bx & 0b0011'1111);
                }

                if (codePoint < minValue || codePoint > 0x10FFFF) {
                    return false;
                }

                out.push_back(codePoint);
                i += static_cast<std::size_t>(length);
            }

            return true;
        }

        bool isAllowedTableNameChar(char32_t ch) {
            const bool isLatinUpper = (ch >= U'A' && ch <= U'Z');
            const bool isLatinLower = (ch >= U'a' && ch <= U'z');
            const bool isDigit = (ch >= U'0' && ch <= U'9');
            const bool isUnderscore = ch == U'_';
            const bool isHyphen = ch == U'-';

            const bool isRussianUpper = (ch >= U'А' && ch <= U'Я') || ch == U'Ё';
            const bool isRussianLower = (ch >= U'а' && ch <= U'я') || ch == U'ё';

            return isLatinUpper
                || isLatinLower
                || isDigit
                || isUnderscore
                || isHyphen
                || isRussianUpper
                || isRussianLower;
        }
    }

    TableName TableName::create(const std::string& value) {
        std::vector<char32_t> chars;
        if (!tryDecodeUtf8(value, chars)) {
            throw KvdbException::invalidTableName("Table name is not valid UTF-8.");
        }

        if (chars.size() < 2 || chars.size() > 127) {
            throw KvdbException::invalidTableName(
                "Table name length must be between 2 and 127 characters."
            );
        }

        for (const auto ch : chars) {
            if (!isAllowedTableNameChar(ch)) {
                throw KvdbException::invalidTableName(
                    "Table name contains unsupported characters."
                );
            }
        }

        return TableName(value);
    }
}
