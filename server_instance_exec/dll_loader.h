#pragma once

#include <filesystem>
#include <string>

namespace kvdb::server_instance_exec {
    class DynamicLibrary final
    {
    public:
        DynamicLibrary() = default;
        explicit DynamicLibrary(const std::filesystem::path& path);
        ~DynamicLibrary();

        DynamicLibrary(const DynamicLibrary&) = delete;
        DynamicLibrary& operator=(const DynamicLibrary&) = delete;

        DynamicLibrary(DynamicLibrary&& other) noexcept;
        DynamicLibrary& operator=(DynamicLibrary&& other) noexcept;

        void load(const std::filesystem::path& path);
        void unload() noexcept;

        [[nodiscard]] bool isLoaded() const noexcept;

        [[nodiscard]] void* getSymbolRaw(const char* symbolName) const;

        template<typename T>
        [[nodiscard]] T getSymbol(const char* symbolName) const {
            return reinterpret_cast<T>(getSymbolRaw(symbolName));
        }

        [[nodiscard]] const std::filesystem::path& path() const noexcept {
            return path_;
        }

    private:
        void* handle_ = nullptr;
        std::filesystem::path path_;
    };

    [[nodiscard]] std::string getLastWindowsErrorMessage();
}