#include "../../query_parser.h"
#include <sstream>

std::vector<Action> parseQuery(const std::string &query) {
    std::stringstream ss(query);

    std::string cmd;
    ss >> cmd;

    std::vector<Action> actions;

    if (cmd == "hello") {
        std::string name;
        ss >> name;

        actions.push_back({
            ActionType::Hello,
            name
        });
    }

    return actions;
}
