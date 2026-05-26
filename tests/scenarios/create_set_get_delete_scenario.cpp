#include "scenario_common.h"

int main() {
    try {
        using namespace kvdb::tests::scenarios;

        ScenarioRunner scenario{
            "Scenario 1: create table, set values, get values, delete value",
            {"scenario_basic"}
        };

        bool scenarioPassed = scenario.cleanBefore();

        scenarioPassed &= scenario.runStep(
            "Create table for storing users",
            R"(CreateTable "scenario_basic" 'charseq(64) To 'charseq(128))"
        );

        scenarioPassed &= scenario.runStep(
            "Add first value",
            R"(In "scenario_basic" Set "user-1" To "John")"
        );

        scenarioPassed &= scenario.runStep(
            "Read first value",
            R"(In "scenario_basic" Get "user-1")"
        );

        scenarioPassed &= scenario.runStep(
            "Overwrite existing value",
            R"(In "scenario_basic" Set "user-1" To "Updated John")"
        );

        scenarioPassed &= scenario.runStep(
            "Read updated value",
            R"(In "scenario_basic" Get "user-1")"
        );

        scenarioPassed &= scenario.runStep(
            "Add second value",
            R"(In "scenario_basic" Set "user-2" To "Alice")"
        );

        scenarioPassed &= scenario.runStep(
            "Read second value",
            R"(In "scenario_basic" Get "user-2")"
        );

        scenarioPassed &= scenario.runStep(
            "Delete first value",
            R"(In "scenario_basic" Delete "user-1")"
        );

        scenarioPassed &= scenario.runStep(
            "Try to read deleted value; error is expected",
            R"(In "scenario_basic" Get "user-1")",
            ExpectedResult::Error
        );

        scenarioPassed &= scenario.cleanAfter();

        scenario.printResult(scenarioPassed);
        return scenarioPassed ? 0 : 1;
    }
    catch (const std::exception& ex) {
        std::cerr << "Scenario failed with exception: " << ex.what() << '\n';
        return 2;
    }
}
