#include "../../../query_parser/query_parser.h"
#include "../../../response_constructor/response_constructor.h"
#include "../../../engine/engine.h"

#include <iostream>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

int main() {

#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    std::cout << "KVDB server started\n";
    std::string line;
    while (true) {
        std::cout << "server> ";
        std::getline(std::cin, line);

        if (line == "exit")
            break;

        auto actions = parseQuery(line);
        std::cout << "actions: " << actions.size() << std::endl;
        auto result = runQuery(actions);
        std::cout << "result: " << result.size() << std::endl;
        auto response = constructResponse(result);

        std::cout << response << std::endl;
    }
}