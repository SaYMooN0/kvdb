#include "dll_loader.h"

#include <stdexcept>
#include <utility>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace kvdb::db_instance {
    namespace {
#ifndef _WIN32
        [[nodiscard]]
        std::string getLastDlErrorMessage() {
            const char* error = dlerror();

            if (error == nullptr) {
                return "No additional information.";
            }

            return error;
        }
#endif
    }

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
        if (this == &other) {
            return *this;
        }

        unload();

        handle_ = std::exchange(other.handle_, nullptr);
        path_ = std::move(other.path_);

        return *this;
    }

    void DynamicLibrary::load(const std::filesystem::path& path) {
        unload();

#ifdef _WIN32
        const auto moduleHandle = LoadLibraryW(path.c_str());

        if (moduleHandle == nullptr) {
            throw std::runtime_error(
                "Failed to load DLL: " + path.string() +
                ". " + getLastWindowsErrorMessage());
        }

        handle_ = reinterpret_cast<void*>(moduleHandle);
#else
        dlerror();

        void* moduleHandle = dlopen(path.string().c_str(), RTLD_NOW | RTLD_LOCAL);

        if (moduleHandle == nullptr) {
            throw std::runtime_error(
                "Failed to load shared library: " + path.string() +
                ". " + getLastDlErrorMessage());
        }

        handle_ = moduleHandle;
#endif

        path_ = path;
    }

    void DynamicLibrary::unload() noexcept {
        if (handle_ == nullptr) {
            return;
        }

#ifdef _WIN32
        FreeLibrary(reinterpret_cast<HMODULE>(handle_));
#else
        dlclose(handle_);
#endif

        handle_ = nullptr;
        path_.clear();
    }

    bool DynamicLibrary::isLoaded() const noexcept {
        return handle_ != nullptr;
    }

    void* DynamicLibrary::getSymbolRaw(const char* symbolName) const {
        if (handle_ == nullptr) {
            throw std::runtime_error("Attempt to resolve symbol from an unloaded library.");
        }

#ifdef _WIN32
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
        dlerror();

        void* symbol = dlsym(handle_, symbolName);

        if (symbol == nullptr) {
            throw std::runtime_error(
                "Failed to resolve symbol '" + std::string(symbolName) +
                "' from shared library: " + path_.string() +
                ". " + getLastDlErrorMessage());
        }

        return symbol;
#endif
    }

    std::string getLastWindowsErrorMessage() {
#ifdef _WIN32
        const DWORD errorCode = GetLastError();

        if (errorCode == 0) {
            return "No additional information.";
        }

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

        if (size == 0 || rawMessage == nullptr) {
            result = "Unknown Windows error.";
        }
        else {
            result.assign(rawMessage, size);
        }

        if (rawMessage != nullptr) {
            LocalFree(rawMessage);
        }

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
