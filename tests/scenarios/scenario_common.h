#pragma once

#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

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

namespace kvdb::tests::scenarios {
    namespace {
        constexpr const char* EngineStateFilePath = "kvdb_engine_state.bin";

        void removeEngineStateFile() {
            std::error_code ec;
            std::filesystem::remove(EngineStateFilePath, ec);
        }

        std::string quoteQueryString(const std::string_view value) {
            std::string result;
            result.reserve(value.size() + 2);
            result.push_back('"');

            for (const char ch : value) {
                switch (ch) {
                    case '"':
                        result += "\\\"";
                        break;

                    case '\\':
                        result += "\\\\";
                        break;

                    case '\n':
                        result += "\\n";
                        break;

                    case '\r':
                        result += "\\r";
                        break;

                    case '\t':
                        result += "\\t";
                        break;

                    default:
                        result.push_back(ch);
                        break;
                }
            }

            result.push_back('"');
            return result;
        }
    }

    struct QueryParserDeleter final
    {
        void operator()(IQueryParser* parser) const {
            if (parser != nullptr) {
                destroy_query_parser(parser);
            }
        }
    };

    struct EngineDeleter final
    {
        void operator()(IEngine* engine) const {
            if (engine != nullptr) {
                destroy_engine(engine);
            }
        }
    };

    struct ResponseConstructorDeleter final
    {
        void operator()(IResponseConstructor* constructor) const {
            if (constructor != nullptr) {
                destroy_response_constructor(constructor);
            }
        }
    };

    using QueryParserPtr = std::unique_ptr<IQueryParser, QueryParserDeleter>;
    using EnginePtr = std::unique_ptr<IEngine, EngineDeleter>;
    using ResponseConstructorPtr =
        std::unique_ptr<IResponseConstructor, ResponseConstructorDeleter>;

    enum class ExpectedResult
    {
        Success,
        Error,
        Any
    };

    class ScenarioModules final
    {
    public:
        ScenarioModules()
            : parser_(create_query_parser()),
              engine_(create_engine()),
              responseConstructor_(create_response_constructor()) {
            removeEngineStateFile();

            if (parser_ == nullptr || engine_ == nullptr || responseConstructor_ == nullptr) {
                throw std::runtime_error("Failed to create one or more kvdb modules.");
            }

            engine_->onInstanceStart();
        }

        ~ScenarioModules() {
            shutdown();

            // Scenarios are integration checks, not long-running instances.
            // After a scenario finishes, no persisted engine state should remain.
            removeEngineStateFile();
        }

        ScenarioModules(const ScenarioModules&) = delete;
        ScenarioModules& operator=(const ScenarioModules&) = delete;

        [[nodiscard]]
        IQueryParser& parser() const {
            return *parser_;
        }

        [[nodiscard]]
        IEngine& engine() const {
            return *engine_;
        }

        [[nodiscard]]
        IResponseConstructor& responseConstructor() const {
            return *responseConstructor_;
        }

        void shutdown() {
            if (engine_ != nullptr && !shutdownCalled_) {
                engine_->onInstanceShutdown();
                shutdownCalled_ = true;
            }
        }

    private:
        QueryParserPtr parser_;
        EnginePtr engine_;
        ResponseConstructorPtr responseConstructor_;
        bool shutdownCalled_ = false;
    };

    [[nodiscard]]
    bool matchesExpectedResult(
        const bool actualSuccess,
        const ExpectedResult expectedResult
    ) {
        if (expectedResult == ExpectedResult::Any) {
            return true;
        }

        return actualSuccess == (expectedResult == ExpectedResult::Success);
    }

    [[nodiscard]]
    bool runCommand(
        ScenarioModules& modules,
        const std::string& label,
        const std::string& description,
        const std::string& rawQuery,
        const ExpectedResult expectedResult = ExpectedResult::Success
    ) {
        std::cout << "\n[" << label << "] " << description << '\n';
        std::cout << "> " << rawQuery << '\n';

        auto parseResult = modules.parser().parse(rawQuery);

        if (std::holds_alternative<CmdParseErr>(parseResult)) {
            const auto& err = std::get<CmdParseErr>(parseResult);

            std::cout << "Parse result: ERROR\n";

            if (err != nullptr) {
                std::cout << modules.responseConstructor().buildErrResponse(*err) << '\n';
            }
            else {
                std::cout << "Parser returned nullptr error.\n";
            }

            return matchesExpectedResult(false, expectedResult);
        }

        auto& cmd = std::get<CmdParseSuccess>(parseResult);

        if (cmd == nullptr) {
            std::cout << "Parse result: ERROR\n";
            std::cout << "Parser returned nullptr command.\n";

            return matchesExpectedResult(false, expectedResult);
        }

        auto execResult = modules.engine().execute(*cmd);

        if (std::holds_alternative<CmdExecErr>(execResult)) {
            const auto& err = std::get<CmdExecErr>(execResult);

            std::cout << "Execute result: ERROR\n";

            if (err != nullptr) {
                std::cout << modules.responseConstructor().buildErrResponse(*err) << '\n';
            }
            else {
                std::cout << "Engine returned nullptr error.\n";
            }

            return matchesExpectedResult(false, expectedResult);
        }

        const auto& success = std::get<SuccessCmdExecResult>(execResult);

        std::cout << "Execute result: SUCCESS\n";
        std::cout << modules.responseConstructor().buildSuccessResponse(success) << '\n';

        return matchesExpectedResult(true, expectedResult);
    }

    class ScenarioRunner final
    {
    public:
        ScenarioRunner(
            std::string title,
            std::vector<std::string> tablesToClean
        )
            : title_(std::move(title)),
              tablesToClean_(std::move(tablesToClean)) {
            std::cout << title_ << '\n';
        }

        ~ScenarioRunner() {
            if (!cleanupAfterCalled_) {
                cleanAfter();
            }
        }

        ScenarioRunner(const ScenarioRunner&) = delete;
        ScenarioRunner& operator=(const ScenarioRunner&) = delete;

        [[nodiscard]]
        bool cleanBefore() {
            return cleanTables("Cleanup before scenario");
        }

        [[nodiscard]]
        bool cleanAfter() {
            cleanupAfterCalled_ = true;
            const bool cleaned = cleanTables("Cleanup after scenario");

            modules_.shutdown();
            removeEngineStateFile();

            return cleaned;
        }

        [[nodiscard]]
        bool runStep(
            const std::string& description,
            const std::string& rawQuery,
            const ExpectedResult expectedResult = ExpectedResult::Success
        ) {
            const std::string label = std::to_string(nextStepNumber_++);
            return runCommand(
                modules_,
                label,
                description,
                rawQuery,
                expectedResult
            );
        }

        void printResult(const bool scenarioPassed) const {
            std::cout << (scenarioPassed ? "SCENARIO PASSED\n" : "SCENARIO FAILED\n");
        }

    private:
        [[nodiscard]]
        bool cleanTables(const std::string& phaseName) {
            std::cout << "\n--- " << phaseName << " ---\n";

            bool cleaned = true;

            cleaned &= runCommand(
                modules_,
                phaseName + ".1",
                "Rollback possible unfinished transaction",
                "Rollback",
                ExpectedResult::Any
            );

            int cleanupStep = 2;
            for (const auto& tableName : tablesToClean_) {
                cleaned &= runCommand(
                    modules_,
                    phaseName + "." + std::to_string(cleanupStep++),
                    "Ensure table is erased: " + tableName,
                    "EnsureErased " + quoteQueryString(tableName),
                    ExpectedResult::Success
                );
            }

            return cleaned;
        }

    private:
        std::string title_;
        std::vector<std::string> tablesToClean_;
        ScenarioModules modules_;
        int nextStepNumber_ = 1;
        bool cleanupAfterCalled_ = false;
    };
}
