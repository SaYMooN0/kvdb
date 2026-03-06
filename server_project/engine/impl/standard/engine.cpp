#include "../../engine.h"

std::vector<ActionResult> runQuery(const std::vector<Action>& actions)
{
    std::vector<ActionResult> results;

    for (const auto& action : actions)
    {
        if (action.type == ActionType::Hello)
        {
            results.push_back({
                true,
                "hello from engine to " + action.param
            });
        }
    }

    return results;
}