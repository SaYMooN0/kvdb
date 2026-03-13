#pragma once
#include <string>

namespace kvdb::contracts {
    class TableName final
    {
    public:
        [[nodiscard]] static TableName create(const std::string& value);

        [[nodiscard]] const std::string& value() const noexcept {
            return value_;
        }

        [[nodiscard]] bool operator==(const TableName& other) const noexcept = default;

    private:
        explicit TableName(std::string value)
            : value_(std::move(value)) {}

    private:
        std::string value_;
    };
}
