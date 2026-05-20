#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <cerrno>
#include <csignal>
#include <unistd.h>
#endif

#include "dll_loader.h"

#include "../modules/contracts/i_modules.h"

namespace fs = std::filesystem;

namespace kvdb::db_instance {
    namespace {
        constexpr const char* kInstanceSettingsFileName = "instance_settings.txt";
        constexpr const char* kInstancePidFileName = "instance.pid";
        constexpr const char* kInstanceStopRequestFileName = "instance.stop";

        struct InstanceSettings final
        {
            std::string engineDllFileName;
            std::string queryParserDllFileName;
            std::string accessInterfaceDllFileName;
            std::string responseConstructorDllFileName;
        };

        [[nodiscard]]
        std::string trim(const std::string& value) {
            const auto isNotSpace = [](unsigned char ch) {
                return !std::isspace(ch);
            };

            const auto beginIt = std::find_if(value.begin(), value.end(), isNotSpace);

            if (beginIt == value.end())
                return {};

            const auto endIt = std::find_if(value.rbegin(), value.rend(), isNotSpace).base();

            return std::string(beginIt, endIt);
        }

        [[nodiscard]]
        fs::path getExecutableDirectory() {
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
            char buffer[4096];

            const ssize_t length = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);

            if (length <= 0)
                return fs::current_path();

            buffer[length] = '\0';

            return fs::path(buffer).parent_path();
#endif
        }

        [[nodiscard]]
        std::uint64_t getCurrentProcessIdPortable() {
#ifdef _WIN32
            return static_cast<std::uint64_t>(GetCurrentProcessId());
#else
            return static_cast<std::uint64_t>(getpid());
#endif
        }

        [[nodiscard]]
        bool isProcessRunning(const std::uint64_t pid) {
#ifdef _WIN32
            if (pid == 0 || pid > std::numeric_limits<DWORD>::max())
                return false;

            HANDLE process = OpenProcess(
                SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION,
                FALSE,
                static_cast<DWORD>(pid)
            );

            if (process == nullptr)
                return false;

            const DWORD waitResult = WaitForSingleObject(process, 0);
            CloseHandle(process);

            return waitResult == WAIT_TIMEOUT;
#else
            if (pid == 0 || pid > static_cast<std::uint64_t>(std::numeric_limits<pid_t>::max()))
                return false;

            const auto nativePid = static_cast<pid_t>(pid);

            if (kill(nativePid, 0) == 0)
                return true;

            return errno == EPERM;
#endif
        }

        [[nodiscard]]
        std::optional<std::uint64_t> readPidFromFile(const fs::path& pidPath) {
            if (!fs::exists(pidPath))
                return std::nullopt;

            std::ifstream input(pidPath);

            if (!input.is_open())
                return std::nullopt;

            std::uint64_t pid = 0;

            if (!(input >> pid))
                return std::nullopt;

            if (pid == 0)
                return std::nullopt;

            return pid;
        }

        bool writePidToFile(const fs::path& pidPath, const std::uint64_t pid) {
            std::ofstream output(pidPath);

            if (!output.is_open())
                return false;

            output << pid << '\n';
            return true;
        }

        class InstanceRuntimeRegistration final
        {
        public:
            explicit InstanceRuntimeRegistration(fs::path instanceDir)
                : instanceDir_(std::move(instanceDir)),
                  pidPath_(instanceDir_ / kInstancePidFileName) {
                const std::optional<std::uint64_t> existingPid = readPidFromFile(pidPath_);

                if (existingPid.has_value()) {
                    if (isProcessRunning(*existingPid)) {
                        throw std::runtime_error(
                            "This instance is already running. PID: " +
                            std::to_string(*existingPid));
                    }

                    std::error_code ec;
                    fs::remove(pidPath_, ec);
                }

                const std::uint64_t currentPid = getCurrentProcessIdPortable();

                if (!writePidToFile(pidPath_, currentPid)) {
                    throw std::runtime_error(
                        "Failed to write instance pid file: " + pidPath_.string());
                }

                registered_ = true;
            }

            ~InstanceRuntimeRegistration() {
                if (!registered_)
                    return;

                std::error_code ec;
                fs::remove(pidPath_, ec);
            }

            InstanceRuntimeRegistration(const InstanceRuntimeRegistration&) = delete;
            InstanceRuntimeRegistration& operator=(const InstanceRuntimeRegistration&) = delete;

            InstanceRuntimeRegistration(InstanceRuntimeRegistration&&) = delete;
            InstanceRuntimeRegistration& operator=(InstanceRuntimeRegistration&&) = delete;

        private:
            fs::path instanceDir_;
            fs::path pidPath_;
            bool registered_ = false;
        };

        [[nodiscard]]
        InstanceSettings readInstanceSettings(const fs::path& settingsPath) {
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
                    "3) access_interface DLL file name\n"
                    "4) response_constructor DLL file name");
            }

            return InstanceSettings{
                .engineDllFileName = values[0],
                .queryParserDllFileName = values[1],
                .accessInterfaceDllFileName = values[2],
                .responseConstructorDllFileName = values[3]
            };
        }

        [[nodiscard]]
        fs::path resolveDllPath(
            const fs::path& executableDir,
            const std::string& dllFileName
        ) {
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

            [[nodiscard]]
            TContract& get() const {
                return *instance_;
            }

        private:
            DynamicLibrary library_;
            CreateFn createFn_ = nullptr;
            DestroyFn destroyFn_ = nullptr;
            TContract* instance_ = nullptr;
        };

        [[nodiscard]]
        fs::path getStopRequestPath(const fs::path& instanceDir) {
            return instanceDir / kInstanceStopRequestFileName;
        }

        void removeStopRequestFile(const fs::path& instanceDir) {
            std::error_code ec;
            fs::remove(getStopRequestPath(instanceDir), ec);
        }

        [[nodiscard]]
        bool hasStopRequest(const fs::path& instanceDir) {
            return fs::exists(getStopRequestPath(instanceDir));
        }
    }

    int run(int argc, char** argv) {
        try {
            const fs::path executableDir = getExecutableDirectory();

            InstanceRuntimeRegistration runtimeRegistration(executableDir);

            fs::path settingsPath;

            if (argc >= 2) {
                settingsPath = fs::path(argv[1]);

                if (settingsPath.is_relative())
                    settingsPath = fs::absolute(settingsPath);
            }
            else {
                settingsPath = executableDir / kInstanceSettingsFileName;
            }

            const InstanceSettings settings = readInstanceSettings(settingsPath);

            const fs::path engineDllPath =
                resolveDllPath(executableDir, settings.engineDllFileName);

            const fs::path queryParserDllPath =
                resolveDllPath(executableDir, settings.queryParserDllFileName);

            const fs::path accessInterfaceDllPath =
                resolveDllPath(executableDir, settings.accessInterfaceDllFileName);

            const fs::path responseConstructorDllPath =
                resolveDllPath(executableDir, settings.responseConstructorDllFileName);

            std::cout << "Loading modules...\n";
            std::cout << "  engine: " << engineDllPath.string() << '\n';
            std::cout << "  query_parser: " << queryParserDllPath.string() << '\n';
            std::cout << "  access_interface: " << accessInterfaceDllPath.string() << '\n';
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

            LoadedModule<kvdb::contracts::IAccessInterface> accessInterfaceModule(
                accessInterfaceDllPath,
                "create_access_interface",
                "destroy_access_interface");

            std::cout << "Starting access interface...\n";

            removeStopRequestFile(executableDir);

            engineModule.get().onInstanceStart();

            std::atomic_bool accessInterfaceStopped = false;
            std::exception_ptr accessInterfaceException = nullptr;

            std::thread accessInterfaceThread([&] {
                try {
                    accessInterfaceModule.get().start(
                        queryParserModule.get(),
                        engineModule.get(),
                        responseConstructorModule.get());
                }
                catch (...) {
                    accessInterfaceException = std::current_exception();
                }

                accessInterfaceStopped = true;
            });

            while (!accessInterfaceStopped) {
                if (hasStopRequest(executableDir)) {
                    std::cout << "Stop request received.\n";

                    removeStopRequestFile(executableDir);
                    accessInterfaceModule.get().requestStop();

                    break;
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(200));
            }

            if (accessInterfaceThread.joinable())
                accessInterfaceThread.join();

            engineModule.get().onInstanceShutdown();

            if (accessInterfaceException != nullptr)
                std::rethrow_exception(accessInterfaceException);

            std::cout << "Access interface stopped.\n";
            return 0;
        }
        catch (const std::exception& ex) {
            std::cerr << "db_instance error: " << ex.what() << '\n';
            return 1;
        }
        catch (...) {
            std::cerr << "db_instance error: unknown exception\n";
            return 1;
        }
    }
}

int main(int argc, char** argv) {
    return kvdb::db_instance::run(argc, argv);
}
