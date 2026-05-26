#include "scenario_common.h"

int main() {
    try {
        using namespace kvdb::tests::scenarios;

        ScenarioRunner scenario{
            "Scenario 2: transaction with Commit",
            {"scenario_commit"}
        };

        bool scenarioPassed = scenario.cleanBefore();

        scenarioPassed &= scenario.runStep(
            "Create table before transaction",
            R"(CreateTable "scenario_commit" 'charseq(64) To 'charseq(128))"
        );

        scenarioPassed &= scenario.runStep(
            "Check that transaction is not active before Begin",
            R"(AnyTransaction)"
        );

        scenarioPassed &= scenario.runStep(
            "Start transaction",
            R"(Begin)"
        );

        scenarioPassed &= scenario.runStep(
            "Check that transaction is active after Begin",
            R"(AnyTransaction)"
        );

        scenarioPassed &= scenario.runStep(
            "Set value inside transaction",
            R"(In "scenario_commit" Set "key-1" To "Value created inside committed transaction")"
        );

        scenarioPassed &= scenario.runStep(
            "Read value before Commit; value should already be visible inside transaction",
            R"(In "scenario_commit" Get "key-1")"
        );

        scenarioPassed &= scenario.runStep(
            "Commit transaction",
            R"(Commit)"
        );

        scenarioPassed &= scenario.runStep(
            "Check that transaction is not active after Commit",
            R"(AnyTransaction)"
        );

        scenarioPassed &= scenario.runStep(
            "Read value after Commit; value must still exist",
            R"(In "scenario_commit" Get "key-1")"
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
