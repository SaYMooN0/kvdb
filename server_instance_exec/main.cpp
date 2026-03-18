#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

#include "dll_loader.h"

#include "../modules/contracts/i_modules.h"

namespace fs = std::filesystem;

namespace kvdb::server_instance_exec {
    namespace {
        struct InstanceSettings final
        {
            std::string engineDllFileName;
            std::string queryParserDllFileName;
            std::string serverDllFileName;
            std::string responseConstructorDllFileName;
        };

        [[nodiscard]] std::string trim(const std::string& value) {
            const auto isNotSpace = [](unsigned char ch) {
                return !std::isspace(ch);
            };

            const auto beginIt = std::find_if(value.begin(), value.end(), isNotSpace);

            if (beginIt == value.end())
                return {};

            const auto endIt = std::find_if(value.rbegin(), value.rend(), isNotSpace).base();

            return std::string(beginIt, endIt);
        }

        [[nodiscard]] fs::path getExecutableDirectory() {
#ifdef _WIN32
            std::wstring buffer(32768, L'\0');

            const DWORD length = GetModuleFileNameW(
                nullptr,
                buffer.data(),
                static_cast<DWORD>(buffer.size()));

            if (length == 0) {
                throw std::runtime_error(
                    "Failed to get executable path. " + getLastWindowsErrorMessage());
            }

            buffer.resize(length);

            return fs::path(buffer).parent_path();
#else
            return fs::current_path();
#endif
        }

        [[nodiscard]] InstanceSettings readInstanceSettings(const fs::path& settingsPath) {
            std::ifstream input(settingsPath);

            if (!input.is_open()) {
                throw std::runtime_error(
                    "Failed to open settings file: " + settingsPath.string());
            }

            std::vector<std::string> values;
            std::string line;

            while (std::getline(input, line)) {
                std::string trimmed = trim(line);

                if (trimmed.empty())
                    continue;

                if (trimmed.starts_with('#'))
                    continue;

                values.push_back(trimmed);
            }

            if (values.size() != 4) {
                throw std::runtime_error(
                    "instance_settings.txt must contain exactly 4 non-empty lines:\n"
                    "1) engine DLL file name\n"
                    "2) query_parser DLL file name\n"
                    "3) server DLL file name\n"
                    "4) response_constructor DLL file name");
            }

            return InstanceSettings{
                .engineDllFileName = values[0],
                .queryParserDllFileName = values[1],
                .serverDllFileName = values[2],
                .responseConstructorDllFileName = values[3]
            };
        }

        [[nodiscard]] fs::path resolveDllPath(
            const fs::path& executableDir,
            const std::string& dllFileName) {
            fs::path result(dllFileName);

            if (result.is_absolute())
                return result;

            return executableDir / result;
        }

        template <typename TContract>
        class LoadedModule final
        {
        public:
            using CreateFn = TContract* (*)();
            using DestroyFn = void (*)(TContract*);

            LoadedModule(
                const fs::path& dllPath,
                const char* createFunctionName,
                const char* destroyFunctionName)
                : library_(dllPath) {
                createFn_ = library_.getSymbol<CreateFn>(createFunctionName);
                destroyFn_ = library_.getSymbol<DestroyFn>(destroyFunctionName);

                instance_ = createFn_();

                if (instance_ == nullptr) {
                    throw std::runtime_error(
                        "Factory '" + std::string(createFunctionName) +
                        "' returned nullptr for DLL: " + dllPath.string());
                }
            }

            ~LoadedModule() {
                if (instance_ != nullptr && destroyFn_ != nullptr) {
                    destroyFn_(instance_);
                    instance_ = nullptr;
                }
            }

            LoadedModule(const LoadedModule&) = delete;
            LoadedModule& operator=(const LoadedModule&) = delete;

            LoadedModule(LoadedModule&&) = delete;
            LoadedModule& operator=(LoadedModule&&) = delete;

            [[nodiscard]] TContract& get() const {
                return *instance_;
            }

        private:
            DynamicLibrary library_;
            CreateFn createFn_ = nullptr;
            DestroyFn destroyFn_ = nullptr;
            TContract* instance_ = nullptr;
        };
    }

    int run(int argc, char** argv) {
        try {
            const fs::path executableDir = getExecutableDirectory();

            fs::path settingsPath;

            if (argc >= 2) {
                settingsPath = fs::path(argv[1]);

                if (settingsPath.is_relative())
                    settingsPath = fs::absolute(settingsPath);
            }
            else {
                settingsPath = executableDir / "instance_settings.txt";
            }

            const InstanceSettings settings = readInstanceSettings(settingsPath);

            const fs::path engineDllPath =
                resolveDllPath(executableDir, settings.engineDllFileName);

            const fs::path queryParserDllPath =
                resolveDllPath(executableDir, settings.queryParserDllFileName);

            const fs::path serverDllPath =
                resolveDllPath(executableDir, settings.serverDllFileName);

            const fs::path responseConstructorDllPath =
                resolveDllPath(executableDir, settings.responseConstructorDllFileName);

            std::cout << "Loading modules...\n";
            std::cout << "  engine: " << engineDllPath.string() << '\n';
            std::cout << "  query_parser: " << queryParserDllPath.string() << '\n';
            std::cout << "  server: " << serverDllPath.string() << '\n';
            std::cout << "  response_constructor: " << responseConstructorDllPath.string() << '\n';

            LoadedModule<kvdb::contracts::IEngine> engineModule(
                engineDllPath,
                "create_engine",
                "destroy_engine");

            LoadedModule<kvdb::contracts::IQueryParser> queryParserModule(
                queryParserDllPath,
                "create_query_parser",
                "destroy_query_parser");

            LoadedModule<kvdb::contracts::IResponseConstructor> responseConstructorModule(
                responseConstructorDllPath,
                "create_response_constructor",
                "destroy_response_constructor");

            LoadedModule<kvdb::contracts::IServer> serverModule(
                serverDllPath,
                "create_server",
                "destroy_server");

            std::cout << "Starting server...\n";

            serverModule.get().start(
                queryParserModule.get(),
                engineModule.get(),
                responseConstructorModule.get());

            std::cout << "Server stopped.\n";
            return 0;
        }
        catch (const std::exception& ex) {
            std::cerr << "server_instance_exec error: " << ex.what() << '\n';
            return 1;
        }
        catch (...) {
            std::cerr << "server_instance_exec error: unknown exception\n";
            return 1;
        }
    }
}

int main(int argc, char** argv) {
    return kvdb::server_instance_exec::run(argc, argv);
}
