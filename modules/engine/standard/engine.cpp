#include <string>

#include "i_modules.h"

namespace kvdb::modules::engine::standard {
    class StandardEngine final : public kvdb::contracts::IEngine
    {
    public:
        std::string execute(const std::string& parsedQuery) override {
            return "engine processed: " + parsedQuery;
        }
    };
}

extern "C" __declspec(dllexport)
kvdb::contracts::IEngine* create_engine() {
    return new kvdb::modules::engine::standard::StandardEngine();
}

extern "C" __declspec(dllexport)
void destroy_engine(kvdb::contracts::IEngine* ptr) {
    delete ptr;
}