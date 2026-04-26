#include "i_modules.h"

#include <algorithm>
#include <cctype>
#include <memory>
#include <string>

namespace kvdb::modules::query_parser::standard {
    namespace {
        std::string trim(const std::string& value) {
            const auto isNotSpace = [](unsigned char ch) {
                return !std::isspace(ch);
            };

            const auto beginIt = std::find_if(
                value.begin(),
                value.end(),
                isNotSpace
            );

            if (beginIt == value.end()) {
                return {};
            }

            const auto endIt = std::find_if(
                value.rbegin(),
                value.rend(),
                isNotSpace
            ).base();

            return std::string(beginIt, endIt);
        }

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

        kvdb::contracts::CmdParseResult makeCmd(
            std::unique_ptr<kvdb::contracts::BaseCmdDto> cmd
        ) {
            return kvdb::contracts::CmdParseSuccess{std::move(cmd)};
        }
    }

    class StandardQueryParser final : public kvdb::contracts::IQueryParser
    {
    public:
        kvdb::contracts::CmdParseResult parse(
            const std::string& rawQuery
        ) override {
            using namespace kvdb::contracts;

            const auto normalized = toLower(trim(rawQuery));

            if (normalized == "begin") {
                return makeCmd(std::make_unique<BeginCmdDto>());
            }

            if (normalized == "commit") {
                return makeCmd(std::make_unique<CommitCmdDto>());
            }

            if (normalized == "rollback") {
                return makeCmd(std::make_unique<RollbackCmdDto>());
            }

            if (
                normalized == "anytransaction"
                || normalized == "any_transaction"
                || normalized == "transaction?"
            ) {
                return makeCmd(std::make_unique<AnyTransactionCmdDto>());
            }

            // Temporary fallback stub.
            return makeCmd(std::make_unique<AnyTransactionCmdDto>());
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