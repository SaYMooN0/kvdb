#pragma once
#include <string>
#include <vector>

enum class ActionType
{
    Hello
};

struct Action
{
    ActionType type;
    std::string param;
};

struct ActionResult
{
    bool isSuccess;
    std::string data;
};