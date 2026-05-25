#include "engine_common.h"

#include <fstream>
#include <limits>
#include <stdexcept>
#include <utility>

namespace kvdb::modules::engine::standard {
template <typename T>
void writePod(std::ostream& out, const T& value) {
    out.write(reinterpret_cast<const char*>(&value), sizeof(T));
    if (!out) {
        throw std::runtime_error("Failed to write engine state.");
    }
}

template <typename T>
T readPod(std::istream& in) {
    T value{};
    in.read(reinterpret_cast<char*>(&value), sizeof(T));
    if (!in) {
        throw std::runtime_error("Failed to read engine state.");
    }

    return value;
}

void writeString(std::ostream& out, const std::string& value) {
    const auto size = static_cast<std::uint64_t>(value.size());
    writePod(out, size);

    out.write(value.data(), static_cast<std::streamsize>(value.size()));
    if (!out) {
        throw std::runtime_error("Failed to write string to engine state.");
    }
}

std::string readString(std::istream& in) {
    const auto size = readPod<std::uint64_t>(in);

    if (size > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
        throw std::runtime_error("String in engine state is too large.");
    }

    std::string result(static_cast<std::size_t>(size), '\0');

    if (!result.empty()) {
        in.read(result.data(), static_cast<std::streamsize>(result.size()));
        if (!in) {
            throw std::runtime_error("Failed to read string from engine state.");
        }
    }

    return result;
}

void writeType(std::ostream& out, const DbType& type) {
    writePod(out, static_cast<std::uint8_t>(type.type));
    writePod(out, type.sizeParam);

    const bool hasTypeParam = type.typeParam != nullptr;
    writePod(out, hasTypeParam);

    if (hasTypeParam) {
        writeType(out, *type.typeParam);
    }
}

DbType readType(std::istream& in) {
    DbType result;
    result.type = static_cast<CmdTypeKind>(readPod<std::uint8_t>(in));
    result.sizeParam = readPod<std::uint16_t>(in);

    const bool hasTypeParam = readPod<bool>(in);
    if (hasTypeParam) {
        result.typeParam = std::make_shared<DbType>(readType(in));
    }

    return result;
}

void writePrimitive(std::ostream& out, const StoredPrimitive& value) {
    writePod(out, static_cast<std::uint8_t>(value.kind));

    switch (value.kind) {
        case StoredPrimitiveKind::Uuid:
            out.write(reinterpret_cast<const char*>(value.uuid.data()), static_cast<std::streamsize>(value.uuid.size()));
            if (!out) {
                throw std::runtime_error("Failed to write uuid.");
            }
            break;

        case StoredPrimitiveKind::CharSeq:
            writeString(out, value.charSeq);
            break;

        case StoredPrimitiveKind::Number:
            writePod(out, value.number.isSigned);
            writePod(out, value.number.byteLength);
            out.write(reinterpret_cast<const char*>(value.number.bytes.data()), value.number.bytes.size());
            if (!out) {
                throw std::runtime_error("Failed to write number.");
            }
            break;

        case StoredPrimitiveKind::Bool:
            writePod(out, value.boolean);
            break;

        case StoredPrimitiveKind::Float:
            writePod(out, value.floating);
            break;
    }
}

StoredPrimitive readPrimitive(std::istream& in) {
    StoredPrimitive result;
    result.kind = static_cast<StoredPrimitiveKind>(readPod<std::uint8_t>(in));

    switch (result.kind) {
        case StoredPrimitiveKind::Uuid:
            in.read(reinterpret_cast<char*>(result.uuid.data()), static_cast<std::streamsize>(result.uuid.size()));
            if (!in) {
                throw std::runtime_error("Failed to read uuid.");
            }
            break;

        case StoredPrimitiveKind::CharSeq:
            result.charSeq = readString(in);
            break;

        case StoredPrimitiveKind::Number:
            result.number.isSigned = readPod<bool>(in);
            result.number.byteLength = readPod<std::uint8_t>(in);
            in.read(reinterpret_cast<char*>(result.number.bytes.data()), static_cast<std::streamsize>(result.number.bytes.size()));
            if (!in) {
                throw std::runtime_error("Failed to read number.");
            }
            break;

        case StoredPrimitiveKind::Bool:
            result.boolean = readPod<bool>(in);
            break;

        case StoredPrimitiveKind::Float:
            result.floating = readPod<double>(in);
            break;
    }

    return result;
}

void writePrimitiveVector(std::ostream& out, const std::vector<StoredPrimitive>& values) {
    writePod(out, static_cast<std::uint64_t>(values.size()));

    for (const auto& value : values) {
        writePrimitive(out, value);
    }
}

std::vector<StoredPrimitive> readPrimitiveVector(std::istream& in) {
    const auto count = readPod<std::uint64_t>(in);

    std::vector<StoredPrimitive> result;
    result.reserve(static_cast<std::size_t>(count));

    for (std::uint64_t i = 0; i < count; ++i) {
        result.push_back(readPrimitive(in));
    }

    return result;
}

void writeNullablePrimitiveVector(
    std::ostream& out,
    const std::vector<std::optional<StoredPrimitive>>& values
) {
    writePod(out, static_cast<std::uint64_t>(values.size()));

    for (const auto& value : values) {
        const bool hasValue = value.has_value();
        writePod(out, hasValue);

        if (hasValue) {
            writePrimitive(out, *value);
        }
    }
}

std::vector<std::optional<StoredPrimitive>> readNullablePrimitiveVector(std::istream& in) {
    const auto count = readPod<std::uint64_t>(in);

    std::vector<std::optional<StoredPrimitive>> result;
    result.reserve(static_cast<std::size_t>(count));

    for (std::uint64_t i = 0; i < count; ++i) {
        const bool hasValue = readPod<bool>(in);

        if (hasValue) {
            result.emplace_back(readPrimitive(in));
        }
        else {
            result.emplace_back(std::nullopt);
        }
    }

    return result;
}

void writeColValue(std::ostream& out, const StoredColValue& value) {
    writePod(out, static_cast<std::uint8_t>(value.kind));

    switch (value.kind) {
        case ColCmdValueKind::Plain:
            writePrimitive(out, value.plain);
            break;

        case ColCmdValueKind::Nullable: {
            const bool hasValue = value.nullable.has_value();
            writePod(out, hasValue);

            if (hasValue) {
                writePrimitive(out, *value.nullable);
            }

            break;
        }

        case ColCmdValueKind::Array:
            writePrimitiveVector(out, value.array);
            break;

        case ColCmdValueKind::ArrayOfNullable:
            writeNullablePrimitiveVector(out, value.arrayOfNullable);
            break;

        case ColCmdValueKind::NullableArray:
            writePod(out, value.nullableArrayHasValue);
            if (value.nullableArrayHasValue) {
                writePrimitiveVector(out, value.nullableArray);
            }
            break;

        case ColCmdValueKind::NullableArrayOfNullable:
            writePod(out, value.nullableArrayOfNullableHasValue);
            if (value.nullableArrayOfNullableHasValue) {
                writeNullablePrimitiveVector(out, value.nullableArrayOfNullable);
            }
            break;
    }
}

StoredColValue readColValue(std::istream& in) {
    StoredColValue result;
    result.kind = static_cast<ColCmdValueKind>(readPod<std::uint8_t>(in));

    switch (result.kind) {
        case ColCmdValueKind::Plain:
            result.plain = readPrimitive(in);
            break;

        case ColCmdValueKind::Nullable: {
            const bool hasValue = readPod<bool>(in);
            if (hasValue) {
                result.nullable = readPrimitive(in);
            }
            else {
                result.nullable = std::nullopt;
            }
            break;
        }

        case ColCmdValueKind::Array:
            result.array = readPrimitiveVector(in);
            break;

        case ColCmdValueKind::ArrayOfNullable:
            result.arrayOfNullable = readNullablePrimitiveVector(in);
            break;

        case ColCmdValueKind::NullableArray:
            result.nullableArrayHasValue = readPod<bool>(in);
            if (result.nullableArrayHasValue) {
                result.nullableArray = readPrimitiveVector(in);
            }
            break;

        case ColCmdValueKind::NullableArrayOfNullable:
            result.nullableArrayOfNullableHasValue = readPod<bool>(in);
            if (result.nullableArrayOfNullableHasValue) {
                result.nullableArrayOfNullable = readNullablePrimitiveVector(in);
            }
            break;
    }

    return result;
}

void saveTablesToFile(const Tables& tables) {
    std::ofstream out(StateFilePath, std::ios::binary | std::ios::trunc);

    if (!out) {
        throw std::runtime_error("Unable to open engine state file for writing.");
    }

    writeString(out, "KVDB_ENGINE_STATE_V1");
    writePod(out, static_cast<std::uint64_t>(tables.size()));

    for (const auto& [tableName, tablePtr] : tables) {
        const Table& table = *tablePtr;

        writeString(out, tableName);
        writeType(out, table.keyType);
        writeType(out, table.valueType);

        writePod(out, static_cast<std::uint64_t>(table.rows.size()));

        for (const auto& [key, value] : table.rows) {
            writePrimitive(out, key);
            writeColValue(out, value);
        }
    }
}

Tables loadTablesFromFile() {
    std::ifstream in(StateFilePath, std::ios::binary);

    if (!in) {
        return {};
    }

    const auto magic = readString(in);
    if (magic != "KVDB_ENGINE_STATE_V1") {
        throw std::runtime_error("Invalid engine state file format.");
    }

    Tables result;

    const auto tableCount = readPod<std::uint64_t>(in);

    for (std::uint64_t i = 0; i < tableCount; ++i) {
        auto tableName = readString(in);

        Table table;
        table.keyType = readType(in);
        table.valueType = readType(in);

        const auto rowCount = readPod<std::uint64_t>(in);

        for (std::uint64_t row = 0; row < rowCount; ++row) {
            auto key = readPrimitive(in);
            auto value = readColValue(in);

            table.rows.emplace(std::move(key), std::move(value));
        }

        result.emplace(
            std::move(tableName),
            std::make_shared<Table>(std::move(table))
        );
    }

    return result;
}
}
