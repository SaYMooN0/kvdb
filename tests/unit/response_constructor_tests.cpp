#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>
#include <variant>

#include "i_modules.h"
#include "err.h"
#include "cmds/cmd_exec_results.h"
#include "cmds/cmd_values.h"

using namespace kvdb::contracts;

extern "C" IResponseConstructor* create_response_constructor();
extern "C" void destroy_response_constructor(IResponseConstructor* constructor);

namespace {
    struct ResponseConstructorDeleter
    {
        void operator()(IResponseConstructor* constructor) const {
            if (constructor != nullptr) {
                destroy_response_constructor(constructor);
            }
        }
    };

    using ResponseConstructorPtr =
        std::unique_ptr<IResponseConstructor, ResponseConstructorDeleter>;

    ResponseConstructorPtr makeResponseConstructorForTests() {
        return ResponseConstructorPtr(create_response_constructor());
    }

    bool contains(
        const std::string& source,
        const std::string& expectedPart
    ) {
        return source.find(expectedPart) != std::string::npos;
    }

    void requireSuccessResponseShape(const std::string& response) {
        INFO("Response: " << response);

        REQUIRE_FALSE(response.empty());
        REQUIRE(contains(response, "isSuccess"));
        REQUIRE(contains(response, "true"));
    }

    void requireErrResponseShape(const std::string& response) {
        INFO("Response: " << response);

        REQUIRE_FALSE(response.empty());
        REQUIRE(contains(response, "isSuccess"));
        REQUIRE(contains(response, "false"));
    }

    ColCmdValue makePlainCharSeqValue(const std::string& value) {
        ColCmdValue result;

        result.kind = ColCmdValueKind::Plain;
        result.plain.kind = PrimitiveCmdValueKind::CharSeq;
        result.plain.charSeq.utf8Value = value.c_str();
        result.plain.charSeq.byteLength = static_cast<std::uint32_t>(value.size());

        return result;
    }

    ColCmdValue makePlainBoolValue(const bool value) {
        ColCmdValue result;

        result.kind = ColCmdValueKind::Plain;
        result.plain.kind = PrimitiveCmdValueKind::Bool;
        result.plain.boolean.value = value;

        return result;
    }

    ColCmdValue makeNullableNullValue() {
        ColCmdValue result;

        result.kind = ColCmdValueKind::Nullable;
        result.nullable.hasValue = false;

        return result;
    }
}

TEST_CASE("response_constructor: buildSuccessResponse handles empty success", "[response_constructor_tests]") {
    auto constructor = makeResponseConstructorForTests();

    REQUIRE(constructor != nullptr);

    const SuccessCmdExecResult success = EmptyCmdExecSuccess{};

    const std::string response = constructor->buildSuccessResponse(success);

    requireSuccessResponseShape(response);
}

TEST_CASE("response_constructor: buildSuccessResponse handles affected rows success", "[response_constructor_tests]") {
    auto constructor = makeResponseConstructorForTests();

    REQUIRE(constructor != nullptr);

    const SuccessCmdExecResult success = AffectedRowsCmdExecSuccess{
        3
    };

    const std::string response = constructor->buildSuccessResponse(success);

    requireSuccessResponseShape(response);

    if (contains(response, "count") || contains(response, "affected")) {
        REQUIRE(contains(response, "3"));
    }
}

TEST_CASE("response_constructor: buildSuccessResponse handles transaction operation success", "[response_constructor_tests]") {
    auto constructor = makeResponseConstructorForTests();

    REQUIRE(constructor != nullptr);

    SECTION("Begin") {
        const SuccessCmdExecResult success = TransactionOpCmdExecSuccess{
            TransactionOpKind::Begin
        };

        const std::string response = constructor->buildSuccessResponse(success);

        requireSuccessResponseShape(response);
    }

    SECTION("Commit") {
        const SuccessCmdExecResult success = TransactionOpCmdExecSuccess{
            TransactionOpKind::Commit
        };

        const std::string response = constructor->buildSuccessResponse(success);

        requireSuccessResponseShape(response);
    }

    SECTION("Rollback") {
        const SuccessCmdExecResult success = TransactionOpCmdExecSuccess{
            TransactionOpKind::Rollback
        };

        const std::string response = constructor->buildSuccessResponse(success);

        requireSuccessResponseShape(response);
    }
}

TEST_CASE("response_constructor: buildSuccessResponse handles transaction check success", "[response_constructor_tests]") {
    auto constructor = makeResponseConstructorForTests();

    REQUIRE(constructor != nullptr);

    SECTION("transaction is active") {
        const SuccessCmdExecResult success = TransactionCheckCmdExecSuccess{
            true
        };

        const std::string response = constructor->buildSuccessResponse(success);

        requireSuccessResponseShape(response);
    }

    SECTION("transaction is inactive") {
        const SuccessCmdExecResult success = TransactionCheckCmdExecSuccess{
            false
        };

        const std::string response = constructor->buildSuccessResponse(success);

        INFO("Response: " << response);

        REQUIRE_FALSE(response.empty());
        REQUIRE(contains(response, "isSuccess"));
        REQUIRE(contains(response, "true"));

        if (contains(response, "transaction") || contains(response, "active")) {
            REQUIRE(contains(response, "false"));
        }
    }
}


TEST_CASE("response_constructor: buildSuccessResponse handles get success with charseq value", "[response_constructor_tests]") {
    auto constructor = makeResponseConstructorForTests();

    REQUIRE(constructor != nullptr);

    const std::string value = "Hello with spaces";

    const ColCmdValue colValue = makePlainCharSeqValue(value);

    const SuccessCmdExecResult success = GetCmdExecSuccess{
        colValue
    };

    const std::string response = constructor->buildSuccessResponse(success);

    INFO("Response: " << response);

    requireSuccessResponseShape(response);

    REQUIRE(contains(response, "get"));
    REQUIRE(contains(response, "value"));
    REQUIRE(contains(response, "charseq"));
    REQUIRE(contains(response, "Hello with spaces"));
}

TEST_CASE("response_constructor: buildSuccessResponse handles get success with nullable null value", "[response_constructor_tests]") {
    auto constructor = makeResponseConstructorForTests();

    REQUIRE(constructor != nullptr);

    const ColCmdValue colValue = makeNullableNullValue();

    const SuccessCmdExecResult success = GetCmdExecSuccess{
        colValue
    };

    const std::string response = constructor->buildSuccessResponse(success);

    INFO("Response: " << response);

    REQUIRE_FALSE(response.empty());
    REQUIRE(contains(response, "isSuccess"));
    REQUIRE(contains(response, "true"));
    REQUIRE(contains(response, "nullable"));
    REQUIRE(contains(response, "hasValue"));
    REQUIRE(contains(response, "false"));
}

TEST_CASE("response_constructor: buildErrResponse includes error data", "[response_constructor_tests]") {
    auto constructor = makeResponseConstructorForTests();

    REQUIRE(constructor != nullptr);

    const InvalidTableNameErr err(
        "bad$name",
        "Table name contains unsupported characters."
    );

    const std::string response = constructor->buildErrResponse(err);

    requireErrResponseShape(response);

    REQUIRE(contains(response, "InvalidTableName"));
    REQUIRE(contains(response, "bad$name"));
    REQUIRE(contains(response, "unsupported characters"));
}

TEST_CASE("response_constructor: buildErrResponse includes module and code", "[response_constructor_tests]") {
    auto constructor = makeResponseConstructorForTests();

    REQUIRE(constructor != nullptr);

    const NullableKeyTypeErr err;

    const std::string response = constructor->buildErrResponse(err);

    requireErrResponseShape(response);

    REQUIRE(contains(response, "contracts"));
    REQUIRE(contains(response, "NullableKeyType"));
    REQUIRE(contains(response, "Key type cannot be nullable"));
}

TEST_CASE("response_constructor: buildErrResponse escapes JSON special characters", "[response_constructor_tests]") {
    auto constructor = makeResponseConstructorForTests();

    REQUIRE(constructor != nullptr);

    const InvalidTableNameErr err(
        "bad\"name",
        "Line 1\nLine 2"
    );

    const std::string response = constructor->buildErrResponse(err);

    requireErrResponseShape(response);

    REQUIRE(contains(response, "InvalidTableName"));

    // В JSON кавычка внутри строки должна быть экранирована как \".
    REQUIRE(contains(response, "\\\""));

    // Перенос строки внутри JSON-строки должен быть экранирован как \n.
    REQUIRE(contains(response, "\\n"));
}

TEST_CASE("response_constructor: buildSessionStartedResponse returns success response", "[response_constructor_tests]") {
    auto constructor = makeResponseConstructorForTests();

    REQUIRE(constructor != nullptr);

    const std::string response = constructor->buildSessionStartedResponse();

    requireSuccessResponseShape(response);
}

TEST_CASE("response_constructor: buildSessionEndedResponse returns success response", "[response_constructor_tests]") {
    auto constructor = makeResponseConstructorForTests();

    REQUIRE(constructor != nullptr);

    const std::string response = constructor->buildSessionEndedResponse();

    requireSuccessResponseShape(response);
}