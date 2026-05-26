#include "scenario_common.h"

int main() {
    try {
        using namespace kvdb::tests::scenarios;

        ScenarioRunner scenario{
            "Scenario 3: transaction with Rollback",
            {"scenario_rollback"}
        };

        bool scenarioPassed = scenario.cleanBefore();

        scenarioPassed &= scenario.runStep(
            "Create table before transaction",
            R"(CreateTable "scenario_rollback" 'charseq(64) To 'charseq(128))"
        );

        scenarioPassed &= scenario.runStep(
            "Create value before transaction; this value must survive rollback",
            R"(In "scenario_rollback" Set "stable-key" To "Stable value before transaction")"
        );

        scenarioPassed &= scenario.runStep(
            "Read stable value before transaction",
            R"(In "scenario_rollback" Get "stable-key")"
        );

        scenarioPassed &= scenario.runStep(
            "Start transaction",
            R"(Begin)"
        );

        scenarioPassed &= scenario.runStep(
            "Add temporary value inside transaction",
            R"(In "scenario_rollback" Set "temporary-key" To "Temporary value")"
        );

        scenarioPassed &= scenario.runStep(
            "Overwrite stable value inside transaction",
            R"(In "scenario_rollback" Set "stable-key" To "Changed inside transaction")"
        );

        scenarioPassed &= scenario.runStep(
            "Read temporary value before Rollback; it is visible inside transaction",
            R"(In "scenario_rollback" Get "temporary-key")"
        );

        scenarioPassed &= scenario.runStep(
            "Rollback transaction",
            R"(Rollback)"
        );

        scenarioPassed &= scenario.runStep(
            "Temporary value must disappear after Rollback; error is expected",
            R"(In "scenario_rollback" Get "temporary-key")",
            ExpectedResult::Error
        );

        scenarioPassed &= scenario.runStep(
            "Stable value must still exist after Rollback",
            R"(In "scenario_rollback" Get "stable-key")"
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
