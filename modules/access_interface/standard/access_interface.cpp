#include <iostream>
#include <string>

#include "i_modules.h"

namespace kvdb::modules::access_interface::standard {
    class StandardAccessInterface final : public kvdb::contracts::IAccessInterface
    {
    public:
        void start(
            kvdb::contracts::IQueryParser& queryParser,
            kvdb::contracts::IEngine& engine,
            kvdb::contracts::IResponseConstructor& responseConstructor) override
        {
            std::cout << "Access Interface started\n";
            std::cout << "Type a query. Type 'exit' to stop.\n";

            std::string rawQuery;

            while (true) {
                std::cout << "> ";

                if (!std::getline(std::cin, rawQuery))
                    break;

                const std::string parsedQuery = queryParser.parse(rawQuery);

                if (parsedQuery == "exit")
                    break;

                const std::string engineResult = engine.execute(parsedQuery);
                const std::string response = responseConstructor.buildResponse(engineResult);

                std::cout << response << '\n';
            }

            std::cout << "Access Interface stopped\n";
        }
    };
}

extern "C" __declspec(dllexport)
kvdb::contracts::IAccessInterface* create_access_interface() {
    return new kvdb::modules::access_interface::standard::StandardAccessInterface();
}

extern "C" __declspec(dllexport)
void destroy_access_interface(kvdb::contracts::IAccessInterface* ptr) {
    delete ptr;
}