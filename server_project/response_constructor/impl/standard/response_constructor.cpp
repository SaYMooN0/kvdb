#include "../../response_constructor.h"

std::string constructResponse(const std::vector<ActionResult>& results)
{
    std::string response;

    for (const auto& r : results)
    {
        response += "|" + r.data + "|";
    }

    return response;
}