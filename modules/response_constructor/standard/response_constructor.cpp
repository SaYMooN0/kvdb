#include <string>

#include "i_modules.h"

namespace kvdb::modules::response_constructor::standard {
    class StandardResponseConstructor final : public kvdb::contracts::IResponseConstructor
    {
    public:
        std::string buildResponse(const std::string& engineResult) override {
            return "OK: " + engineResult;
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