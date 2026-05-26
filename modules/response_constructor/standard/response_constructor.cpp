#include "i_modules.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>
#include "module_export.h"

namespace kvdb::modules::response_constructor::standard {
    namespace {
        using namespace kvdb::contracts;

        std::string escapeJsonString(std::string_view value) {
            std::string result;
            result.reserve(value.size());

            constexpr char hex[] = "0123456789abcdef";

            for (const unsigned char ch : value) {
                switch (ch) {
                case '"':
                    result += "\\\"";
                    break;

                case '\\':
                    result += "\\\\";
                    break;

                case '\b':
                    result += "\\b";
                    break;

                case '\f':
                    result += "\\f";
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
                    if (ch < 0x20) {
                        result += "\\u00";
                        result += hex[(ch >> 4) & 0x0F];
                        result += hex[ch & 0x0F];
                    }
                    else {
                        result += static_cast<char>(ch);
                    }

                    break;
                }
            }

            return result;
        }

        std::string jsonString(std::string_view value) {
            return "\"" + escapeJsonString(value) + "\"";
        }

        std::string jsonBool(const bool value) {
            return value ? "true" : "false";
        }

        std::string jsonDouble(const double value) {
            if (!std::isfinite(value)) {
                return "null";
            }

            std::ostringstream out;
            out << std::setprecision(17) << value;
            return out.str();
        }

        template <typename... Ts>
        struct Overloaded : Ts...
        {
            using Ts::operator()...;
        };

        template <typename... Ts>
        Overloaded(Ts...) -> Overloaded<Ts...>;

        std::string toHexByte(const std::uint8_t byte) {
            constexpr char hex[] = "0123456789abcdef";

            std::string result;
            result += hex[(byte >> 4) & 0x0F];
            result += hex[byte & 0x0F];

            return result;
        }

        std::string uuidToString(const UuidCmdValue& value) {
            std::string result;
            result.reserve(36);

            for (std::size_t i = 0; i < value.bytes.size(); ++i) {
                if (i == 4 || i == 6 || i == 8 || i == 10) {
                    result += '-';
                }

                result += toHexByte(value.bytes[i]);
            }

            return result;
        }

        std::string bytesToHex(
            const std::array<std::uint8_t, 16>& bytes,
            const std::uint8_t byteLength
        ) {
            std::string result;
            result.reserve(static_cast<std::size_t>(byteLength) * 2);

            const auto count = static_cast<std::size_t>(byteLength);
            for (std::size_t i = 0; i < count && i < bytes.size(); ++i) {
                result += toHexByte(bytes[i]);
            }

            return result;
        }

        std::uint64_t readUnsignedNumber(const NumberCmdValue& value) {
            const auto byteLength = static_cast<std::size_t>(value.byteLength);

            std::uint64_t result = 0;
            for (std::size_t i = 0; i < byteLength; ++i) {
                result |= static_cast<std::uint64_t>(value.bytes[i]) << (i * 8U);
            }

            return result;
        }

        std::int64_t readSignedNumber(const NumberCmdValue& value) {
            const auto byteLength = static_cast<std::size_t>(value.byteLength);

            if (byteLength == 0) {
                return 0;
            }

            auto raw = readUnsignedNumber(value);

            const auto signBit = std::uint64_t{1} << (byteLength * 8U - 1U);
            if ((raw & signBit) != 0 && byteLength < 8) {
                const auto extensionMask =
                    ~((std::uint64_t{1} << (byteLength * 8U)) - 1U);

                raw |= extensionMask;
            }

            return static_cast<std::int64_t>(raw);
        }

        std::string numberToJsonValue(const NumberCmdValue& value) {
            if (value.byteLength <= 8) {
                if (value.isSigned) {
                    return std::to_string(readSignedNumber(value));
                }

                return std::to_string(readUnsignedNumber(value));
            }

            return "{"
                R"("representation":"bytes",)"
                R"("isSigned":)" + jsonBool(value.isSigned) + ","
                R"("byteLength":)" + std::to_string(value.byteLength) + ","
                R"("bytesHex":)" + jsonString(bytesToHex(value.bytes, value.byteLength))
                + "}";
        }

        std::string transactionOpKindToString(const TransactionOpKind operation) {
            switch (operation) {
            case TransactionOpKind::Begin:
                return "begin";

            case TransactionOpKind::Commit:
                return "commit";

            case TransactionOpKind::Rollback:
                return "rollback";
            }

            return "unknown";
        }

        std::string typeKindToString(const CmdTypeKind type) {
            switch (type) {
            case CmdTypeKind::Uuid:
                return "uuid";

            case CmdTypeKind::CharSeq:
                return "charseq";

            case CmdTypeKind::Int:
                return "int";

            case CmdTypeKind::UInt:
                return "uint";

            case CmdTypeKind::Bool:
                return "bool";

            case CmdTypeKind::Float:
                return "float";

            case CmdTypeKind::Nullable:
                return "nullable";

            case CmdTypeKind::Array:
                return "array";
            }

            return "unknown";
        }

        std::string primitiveValueToJson(const PrimitiveCmdValue& value) {
            switch (value.kind) {
            case PrimitiveCmdValueKind::Uuid:
                return "{"
                    R"("kind":"uuid",)"
                    R"("value":)" + jsonString(uuidToString(value.uuid))
                    + "}";

            case PrimitiveCmdValueKind::CharSeq:
                return "{"
                    R"("kind":"charseq",)"
                    R"("value":)" + jsonString(
                        std::string_view{
                            value.charSeq.utf8Value,
                            value.charSeq.byteLength
                        }
                    )
                    + "}";

            case PrimitiveCmdValueKind::Number:
                return "{"
                    R"("kind":"number",)"
                    R"("value":)" + numberToJsonValue(value.number) + ","
                    R"("isSigned":)" + jsonBool(value.number.isSigned) + ","
                    R"("byteLength":)" + std::to_string(value.number.byteLength)
                    + "}";

            case PrimitiveCmdValueKind::Bool:
                return "{"
                    R"("kind":"bool",)"
                    R"("value":)" + jsonBool(value.boolean.value)
                    + "}";

            case PrimitiveCmdValueKind::Float:
                return "{"
                    R"("kind":"float",)"
                    R"("value":)" + jsonDouble(value.floating.value)
                    + "}";
            }

            return R"({"kind":"unknown","value":null})";
        }

        std::string nullablePrimitiveValueToJson(
            const NullablePrimitiveCmdValue& value
        ) {
            if (!value.hasValue) {
                return "null";
            }

            return primitiveValueToJson(value.value);
        }

        std::string primitiveArrayToJson(const PrimitiveCmdValueArrayView& array) {
            std::string result = "[";

            for (std::uint32_t i = 0; i < array.count; ++i) {
                if (i != 0) {
                    result += ",";
                }

                result += primitiveValueToJson(array.items[i]);
            }

            result += "]";
            return result;
        }

        std::string nullablePrimitiveArrayToJson(
            const NullablePrimitiveCmdValueArrayView& array
        ) {
            std::string result = "[";

            for (std::uint32_t i = 0; i < array.count; ++i) {
                if (i != 0) {
                    result += ",";
                }

                result += nullablePrimitiveValueToJson(array.items[i]);
            }

            result += "]";
            return result;
        }

        std::string colValueToJson(const ColCmdValue& value) {
            switch (value.kind) {
            case ColCmdValueKind::Plain:
                return "{"
                    R"("kind":"plain",)"
                    R"("value":)" + primitiveValueToJson(value.plain)
                    + "}";

            case ColCmdValueKind::Nullable:
                return "{"
                    R"("kind":"nullable",)"
                    R"("hasValue":)" + jsonBool(value.nullable.hasValue) + ","
                    R"("value":)" + nullablePrimitiveValueToJson(value.nullable)
                    + "}";

            case ColCmdValueKind::Array:
                return "{"
                    R"("kind":"array",)"
                    R"("items":)" + primitiveArrayToJson(value.array)
                    + "}";

            case ColCmdValueKind::ArrayOfNullable:
                return "{"
                    R"("kind":"arrayOfNullable",)"
                    R"("items":)" + nullablePrimitiveArrayToJson(value.arrayOfNullable)
                    + "}";

            case ColCmdValueKind::NullableArray:
                return "{"
                    R"("kind":"nullableArray",)"
                    R"("hasValue":)" + jsonBool(value.nullableArray.hasValue) + ","
                    R"("items":)" + (
                        value.nullableArray.hasValue
                            ? primitiveArrayToJson(value.nullableArray.value)
                            : "null"
                    )
                    + "}";

            case ColCmdValueKind::NullableArrayOfNullable:
                return "{"
                    R"("kind":"nullableArrayOfNullable",)"
                    R"("hasValue":)" + jsonBool(value.nullableArrayOfNullable.hasValue) + ","
                    R"("items":)" + (
                        value.nullableArrayOfNullable.hasValue
                            ? nullablePrimitiveArrayToJson(value.nullableArrayOfNullable.value)
                            : "null"
                    )
                    + "}";
            }

            return R"({"kind":"unknown","value":null})";
        }

        std::string cmdTypeToJson(const CmdTypeKindValue& value) {
            std::string result = "{";
            result += R"("kind":)" + jsonString(typeKindToString(value.type));

            if (value.type == CmdTypeKind::CharSeq
                || value.type == CmdTypeKind::Int
                || value.type == CmdTypeKind::UInt) {
                result += R"(,"size":)" + std::to_string(value.sizeParam);
            }

            if (value.type == CmdTypeKind::Nullable
                || value.type == CmdTypeKind::Array) {
                result += R"(,"inner":)";

                if (value.typeParam == nullptr) {
                    result += "null";
                }
                else {
                    result += cmdTypeToJson(*value.typeParam);
                }
            }

            result += "}";
            return result;
        }

        std::string successToJson(const EmptyCmdExecSuccess&) {
            return R"({"kind":"empty"})";
        }

        std::string successToJson(const TransactionOpCmdExecSuccess& success) {
            return "{"
                R"("kind":"transactionOp",)"
                R"("operation":)" + jsonString(transactionOpKindToString(success.operation))
                + "}";
        }

        std::string successToJson(const TransactionCheckCmdExecSuccess& success) {
            return "{"
                R"("kind":"transactionCheck",)"
                R"("transactionActive":)" + jsonBool(success.transactionActive)
                + "}";
        }

        std::string successToJson(const AffectedRowsCmdExecSuccess& success) {
            return "{"
                R"("kind":"affectedRows",)"
                R"("count":)" + std::to_string(success.count)
                + "}";
        }

        std::string successToJson(const GetCmdExecSuccess& success) {
            return "{"
                R"("kind":"get",)"
                R"("value":)" + colValueToJson(success.value)
                + "}";
        }

        std::string successToJson(const TableInfoCmdExecSuccess& success) {
            return "{"
                R"("kind":"tableInfo",)"
                R"("tableName":)" + jsonString(success.tableName.value()) + ","
                R"("keyType":)" + cmdTypeToJson(success.keyColType) + ","
                R"("valueType":)" + cmdTypeToJson(success.valueColType) + ","
                R"("rowsCount":)" + std::to_string(success.rowsCount)
                + "}";
        }

        std::string successVariantToJson(const SuccessCmdExecResult& success) {
            return std::visit(
                [](const auto& concreteSuccess) {
                    return successToJson(concreteSuccess);
                },
                success
            );
        }
    }

    class StandardResponseConstructor final
        : public kvdb::contracts::IResponseConstructor
    {
    public:
        std::string buildSuccessResponse(
            const kvdb::contracts::SuccessCmdExecResult& success
        ) override {
            return std::string(R"({"isSuccess":true,"data":)")
                + successVariantToJson(success)
                + "}";
        }

        std::string buildErrResponse(
            const kvdb::contracts::Err& err
        ) override {
            const std::string errCode =
                std::string(err.module())
                + "."
                + std::string(err.code());

            return std::string(R"({"isSuccess":false,"errCode":")")
                + escapeJsonString(errCode)
                + R"(","message":")"
                + escapeJsonString(err.message())
                + R"("})";
        }

        std::string buildSessionStartedResponse() override {
            return R"({"isSuccess":true,"data":{"kind":"sessionStarted"}})";
        }

        std::string buildSessionEndedResponse() override {
            return R"({"isSuccess":true,"data":{"kind":"sessionEnded"}})";
        }
    };
}

extern "C" KVDB_MODULE_EXPORT
kvdb::contracts::IResponseConstructor* create_response_constructor() {
    return new kvdb::modules::response_constructor::standard::StandardResponseConstructor();
}

extern "C" KVDB_MODULE_EXPORT
void destroy_response_constructor(kvdb::contracts::IResponseConstructor* ptr) {
    delete ptr;
}
