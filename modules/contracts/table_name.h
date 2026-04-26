#pragma once

#include <memory>
#include <string>
#include <utility>
#include <variant>

#include "err.h"

namespace kvdb::contracts {
    class TableName final
    {
    public:
        using Creation = std::variant<TableName, std::shared_ptr<const Err>>;

        [[nodiscard]]
        static Creation create(std::string value);

        [[nodiscard]]
        static TableName createUnsafe(std::string value) {
            return TableName(std::move(value));
        }

        [[nodiscard]]
        const std::string& value() const noexcept {
            return value_;
        }

        [[nodiscard]]
        bool operator==(const TableName& other) const noexcept = default;

    private:
        explicit TableName(std::string value)
            : value_(std::move(value)) {}

    private:
        std::string value_;
    };
}