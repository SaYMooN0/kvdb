#include "dll_loader.h"

#include <stdexcept>
#include <utility>

#ifdef _WIN32
#include <windows.h>
#endif

namespace kvdb::server_instance_exec {
    DynamicLibrary::DynamicLibrary(const std::filesystem::path& path) {
        load(path);
    }

    DynamicLibrary::~DynamicLibrary() {
        unload();
    }

    DynamicLibrary::DynamicLibrary(DynamicLibrary&& other) noexcept
        : handle_(std::exchange(other.handle_, nullptr)),
          path_(std::move(other.path_)) {}

    DynamicLibrary& DynamicLibrary::operator=(DynamicLibrary&& other) noexcept {
        if (this == &other)
            return *this;

        unload();

        handle_ = std::exchange(other.handle_, nullptr);
        path_ = std::move(other.path_);

        return *this;
    }

    void DynamicLibrary::load(const std::filesystem::path& path) {
#ifdef _WIN32
        unload();

        const auto moduleHandle = LoadLibraryW(path.c_str());

        if (moduleHandle == nullptr) {
            throw std::runtime_error(
                "Failed to load DLL: " + path.string() +
                ". " + getLastWindowsErrorMessage());
        }

        handle_ = reinterpret_cast<void*>(moduleHandle);
        path_ = path;
#else
        throw std::runtime_error("DynamicLibrary is implemented only for Windows in this project.");
#endif
    }

    void DynamicLibrary::unload() noexcept {
#ifdef _WIN32
        if (handle_ != nullptr) {
            FreeLibrary(reinterpret_cast<HMODULE>(handle_));
            handle_ = nullptr;
            path_.clear();
        }
#endif
    }

    bool DynamicLibrary::isLoaded() const noexcept {
        return handle_ != nullptr;
    }

    void* DynamicLibrary::getSymbolRaw(const char* symbolName) const {
#ifdef _WIN32
        if (handle_ == nullptr) {
            throw std::runtime_error("Attempt to resolve symbol from an unloaded library.");
        }

        const auto symbol = GetProcAddress(
            reinterpret_cast<HMODULE>(handle_),
            symbolName);

        if (symbol == nullptr) {
            throw std::runtime_error(
                "Failed to resolve symbol '" + std::string(symbolName) +
                "' from DLL: " + path_.string() +
                ". " + getLastWindowsErrorMessage());
        }

        return reinterpret_cast<void*>(symbol);
#else
        throw std::runtime_error("DynamicLibrary is implemented only for Windows in this project.");
#endif
    }

    std::string getLastWindowsErrorMessage() {
#ifdef _WIN32
        const DWORD errorCode = GetLastError();

        if (errorCode == 0)
            return "No additional information.";

        LPSTR rawMessage = nullptr;

        const DWORD size = FormatMessageA(
            FORMAT_MESSAGE_ALLOCATE_BUFFER |
            FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr,
            errorCode,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            reinterpret_cast<LPSTR>(&rawMessage),
            0,
            nullptr);

        std::string result;

        if (size == 0 || rawMessage == nullptr)
            result = "Unknown Windows error.";
        else
            result.assign(rawMessage, size);

        if (rawMessage != nullptr)
            LocalFree(rawMessage);

        while (!result.empty() &&
            (result.back() == '\n' || result.back() == '\r')) {
            result.pop_back();
        }

        return result;
#else
        return "No Windows error information available.";
#endif
    }
}
