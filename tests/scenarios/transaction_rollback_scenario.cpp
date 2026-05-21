#include <iostream>
#include <memory>
#include <string>
#include <variant>

#include "i_modules.h"
#include "cmds/cmd_dtos.h"
#include "cmds/cmd_parse_results.h"
#include "cmds/cmd_exec_results.h"

using namespace kvdb::contracts;

extern "C" IQueryParser* create_query_parser();
extern "C" void destroy_query_parser(IQueryParser* parser);

extern "C" IEngine* create_engine();
extern "C" void destroy_engine(IEngine* engine);

extern "C" IResponseConstructor* create_response_constructor();
extern "C" void destroy_response_constructor(IResponseConstructor* constructor);

namespace {
    struct QueryParserDeleter
    {
        void operator()(IQueryParser* parser) const {
            if (parser != nullptr) {
                destroy_query_parser(parser);
            }
        }
    };

    struct EngineDeleter
    {
        void operator()(IEngine* engine) const {
            if (engine != nullptr) {
                destroy_engine(engine);
            }
        }
    };

    struct ResponseConstructorDeleter
    {
        void operator()(IResponseConstructor* constructor) const {
            if (constructor != nullptr) {
                destroy_response_constructor(constructor);
            }
        }
    };

    using QueryParserPtr = std::unique_ptr<IQueryParser, QueryParserDeleter>;
    using EnginePtr = std::unique_ptr<IEngine, EngineDeleter>;
    using ResponseConstructorPtr = std::unique_ptr<IResponseConstructor, ResponseConstructorDeleter>;

    enum class ExpectedResult
    {
        Success,
        Error
    };

    struct ScenarioModules
    {
        QueryParserPtr parser{create_query_parser()};
        EnginePtr engine{create_engine()};
        ResponseConstructorPtr responseConstructor{create_response_constructor()};

        ScenarioModules() {
            if (parser == nullptr || engine == nullptr || responseConstructor == nullptr) {
                throw std::runtime_error("Failed to create one or more kvdb modules.");
            }

            engine->onInstanceStart();
        }

        ~ScenarioModules() {
            if (engine != nullptr) {
                engine->onInstanceShutdown();
            }
        }
    };

    void printHeader(const std::string& title) {
        std::cout << title << '\n';
    }

    bool runCommand(
        ScenarioModules& modules,
        int step,
        const std::string& description,
        const std::string& rawQuery,
        ExpectedResult expectedResult = ExpectedResult::Success
    ) {
        std::cout << "\n[" << step << "] " << description << '\n';
        std::cout << "> " << rawQuery << '\n';

        auto parseResult = modules.parser->parse(rawQuery);

        if (std::holds_alternative<CmdParseErr>(parseResult)) {
            const auto& err = std::get<CmdParseErr>(parseResult);

            std::cout << "Parse result: ERROR\n";

            if (err != nullptr) {
                std::cout << modules.responseConstructor->buildErrResponse(*err) << '\n';
            }
            else {
                std::cout << "Parser returned nullptr error.\n";
            }

            return expectedResult == ExpectedResult::Error;
        }

        auto& cmd = std::get<CmdParseSuccess>(parseResult);

        if (cmd == nullptr) {
            std::cout << "Parse result: ERROR\n";
            std::cout << "Parser returned nullptr command.\n";

            return expectedResult == ExpectedResult::Error;
        }

        auto execResult = modules.engine->execute(*cmd);

        if (std::holds_alternative<CmdExecErr>(execResult)) {
            const auto& err = std::get<CmdExecErr>(execResult);

            std::cout << "Execute result: ERROR\n";

            if (err != nullptr) {
                std::cout << modules.responseConstructor->buildErrResponse(*err) << '\n';
            }
            else {
                std::cout << "Engine returned nullptr error.\n";
            }

            return expectedResult == ExpectedResult::Error;
        }

        const auto& success = std::get<SuccessCmdExecResult>(execResult);

        std::cout << "Execute result: SUCCESS\n";
        std::cout << modules.responseConstructor->buildSuccessResponse(success) << '\n';

        return expectedResult == ExpectedResult::Success;
    }
}

int main() {
    try {
        printHeader("Scenario 3: transaction with Rollback");

        ScenarioModules modules;

        bool scenarioPassed = true;

        scenarioPassed &= runCommand(
            modules,
            1,
            "Create table before transaction",
            R"(Create "scenario_rollback" 'charseq(64) To 'charseq(128))"
        );

        scenarioPassed &= runCommand(
            modules,
            2,
            "Create value before transaction; this value must survive rollback",
            R"(In "scenario_rollback" Set "stable-key" To "Stable value before transaction")"
        );

        scenarioPassed &= runCommand(
            modules,
            3,
            "Read stable value before transaction",
            R"(In "scenario_rollback" Get "stable-key")"
        );

        scenarioPassed &= runCommand(
            modules,
            4,
            "Start transaction",
            R"(Begin)"
        );

        scenarioPassed &= runCommand(
            modules,
            5,
            "Add temporary value inside transaction",
            R"(In "scenario_rollback" Set "temporary-key" To "Temporary value")"
        );

        scenarioPassed &= runCommand(
            modules,
            6,
            "Overwrite stable value inside transaction",
            R"(In "scenario_rollback" Set "stable-key" To "Changed inside transaction")"
        );

        scenarioPassed &= runCommand(
            modules,
            7,
            "Read temporary value before Rollback; it is visible inside transaction",
            R"(In "scenario_rollback" Get "temporary-key")"
        );

        scenarioPassed &= runCommand(
            modules,
            8,
            "Rollback transaction",
            R"(Rollback)"
        );

        scenarioPassed &= runCommand(
            modules,
            9,
            "Temporary value must disappear after Rollback; error is expected",
            R"(In "scenario_rollback" Get "temporary-key")",
            ExpectedResult::Error
        );

        scenarioPassed &= runCommand(
            modules,
            10,
            "Stable value must still exist after Rollback",
            R"(In "scenario_rollback" Get "stable-key")"
        );

        std::cout << (scenarioPassed ? "SCENARIO PASSED\n" : "SCENARIO FAILED\n");

        return scenarioPassed ? 0 : 1;
    }
    catch (const std::exception& ex) {
        std::cerr << "Scenario failed with exception: " << ex.what() << '\n';
        return 2;
    }
}