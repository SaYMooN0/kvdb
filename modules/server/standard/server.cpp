#include <iostream>
#include <string>

#include "i_modules.h"

namespace kvdb::modules::server::standard {
    class StandardServer final : public kvdb::contracts::IServer
    {
    public:
        void start(
            kvdb::contracts::IQueryParser& queryParser,
            kvdb::contracts::IEngine& engine,
            kvdb::contracts::IResponseConstructor& responseConstructor) override
        {
            std::cout << "Server started\n";
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

            std::cout << "Server stopped\n";
        }
    };
}

extern "C" __declspec(dllexport)
kvdb::contracts::IServer* create_server() {
    return new kvdb::modules::server::standard::StandardServer();
}

extern "C" __declspec(dllexport)
void destroy_server(kvdb::contracts::IServer* ptr) {
    delete ptr;
}