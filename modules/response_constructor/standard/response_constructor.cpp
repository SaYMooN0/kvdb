#include "i_modules.h"

#include <string>
#include <string_view>

namespace kvdb::modules::response_constructor::standard {
    namespace {
        std::string escapeJsonString(std::string_view value) {
            std::string result;
            result.reserve(value.size());

            for (const char ch : value) {
                switch (ch) {
                    case '"':
                        result += "\\\"";
                        break;

                    case '\\':
                        result += "\\\\";
                        break;

                    case '\b':
                        result += "\\b";
                        break;

                    case '\f':
                        result += "\\f";
                        break;

                    case '\n':
                        result += "\\n";
                        break;

                    case '\r':
                        result += "\\r";
                        break;

                    case '\t':
                        result += "\\t";
                        break;

                    default:
                        result += ch;
                        break;
                }
            }

            return result;
        }
    }

    class StandardResponseConstructor final
        : public kvdb::contracts::IResponseConstructor
    {
    public:
        std::string buildSuccessResponse(
            const kvdb::contracts::SuccessCmdExecResult& success
        ) override {
            (void)success;

            return R"({"isSuccess":true})";
        }

        std::string buildErrResponse(
            const kvdb::contracts::Err& err
        ) override {
            const std::string errCode =
                std::string(err.module())
                + "."
                + std::string(err.code());

            return std::string(R"({"isSuccess":false,"errCode":")")
                + escapeJsonString(errCode)
                + R"(","message":")"
                + escapeJsonString(err.message())
                + R"("})";
        }

        std::string buildSessionStartedResponse() override {
            return R"({"isSuccess":true})";
        }

        std::string buildSessionEndedResponse() override {
            return R"({"isSuccess":true})";
        }
    };
}

extern "C" __declspec(dllexport)
kvdb::contracts::IResponseConstructor* create_response_constructor() {
    return new kvdb::modules::response_constructor::standard::StandardResponseConstructor();
}

extern "C" __declspec(dllexport)
void destroy_response_constructor(kvdb::contracts::IResponseConstructor* ptr) {
    delete ptr;
}