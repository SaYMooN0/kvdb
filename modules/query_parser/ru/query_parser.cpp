#include <algorithm>
#include <cctype>
#include <string>

#include "i_modules.h"

namespace kvdb::modules::query_parser::ru {
    namespace {
        std::string trim(const std::string& value) {
            const auto isNotSpace = [](unsigned char ch) {
                return !std::isspace(ch);
            };

            const auto beginIt = std::find_if(value.begin(), value.end(), isNotSpace);

            if (beginIt == value.end())
                return {};

            const auto endIt = std::find_if(value.rbegin(), value.rend(), isNotSpace).base();

            return std::string(beginIt, endIt);
        }
    }

    class RussianQueryParser final : public kvdb::contracts::IQueryParser
    {
    public:
        std::string parse(const std::string& rawQuery) override {
            std::string q = trim(rawQuery);

            if (q == "создать таблицу")
                return "create table";

            if (q == "удалить таблицу")
                return "erase table";

            if (q == "выход")
                return "exit";

            return "[ru] " + q;
        }
    };
}

extern "C" __declspec(dllexport)
kvdb::contracts::IQueryParser* create_query_parser() {
    return new kvdb::modules::query_parser::ru::RussianQueryParser();
}

extern "C" __declspec(dllexport)
void destroy_query_parser(kvdb::contracts::IQueryParser* ptr) {
    delete ptr;
}