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
        printHeader("Scenario 1: create table, set values, get values, delete value");

        ScenarioModules modules;

        bool scenarioPassed = true;

        scenarioPassed &= runCommand(
            modules,
            1,
            "Create table for storing users",
            R"(Create "scenario_basic" 'charseq(64) To 'charseq(128))"
        );

        scenarioPassed &= runCommand(
            modules,
            2,
            "Add first value",
            R"(In "scenario_basic" Set "user-1" To "John")"
        );

        scenarioPassed &= runCommand(
            modules,
            3,
            "Read first value",
            R"(In "scenario_basic" Get "user-1")"
        );

        scenarioPassed &= runCommand(
            modules,
            4,
            "Overwrite existing value",
            R"(In "scenario_basic" Set "user-1" To "Updated John")"
        );

        scenarioPassed &= runCommand(
            modules,
            5,
            "Read updated value",
            R"(In "scenario_basic" Get "user-1")"
        );

        scenarioPassed &= runCommand(
            modules,
            6,
            "Add second value",
            R"(In "scenario_basic" Set "user-2" To "Alice")"
        );

        scenarioPassed &= runCommand(
            modules,
            7,
            "Read second value",
            R"(In "scenario_basic" Get "user-2")"
        );

        scenarioPassed &= runCommand(
            modules,
            8,
            "Delete first value",
            R"(In "scenario_basic" Del "user-1")"
        );

        scenarioPassed &= runCommand(
            modules,
            9,
            "Try to read deleted value; error is expected",
            R"(In "scenario_basic" Get "user-1")",
            ExpectedResult::Error
        );

        std::cout << (scenarioPassed ? "SCENARIO PASSED\n" : "SCENARIO FAILED\n");

        return scenarioPassed ? 0 : 1;
    }
    catch (const std::exception& ex) {
        std::cerr << "Scenario failed with exception: " << ex.what() << '\n';
        return 2;
    }
}