#include <filesystem>
#include <fstream>
#include <cctype>
#include <chrono>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <cerrno>
#include <csignal>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;

#ifndef KVDB_SOURCE_DIR
#define KVDB_SOURCE_DIR "."
#endif

#ifndef KVDB_DIST_DIR
#define KVDB_DIST_DIR "./dist"
#endif

#ifndef KVDB_INSTANCES_ROOT_DIR
#ifdef _WIN32
#define KVDB_INSTANCES_ROOT_DIR "."
#else
#define KVDB_INSTANCES_ROOT_DIR "/data"
#endif
#endif

struct Instance
{
    std::string name;
    std::string path;
};

struct InstanceRunState
{
    bool isRunning = false;
    std::optional<std::uint64_t> pid;
    std::optional<std::uint16_t> port;
};

#ifdef _WIN32
constexpr const char* kDbInstanceFileName = "db_instance.exe";
constexpr const char* kModuleExtension = ".dll";
#else
constexpr const char* kDbInstanceFileName = "db_instance";
constexpr const char* kModuleExtension = ".so";
#endif

constexpr const char* kInstanceSettingsFileName = "instance_settings.txt";
constexpr const char* kInstancesRegistryFileName = "manager_instances.txt";
constexpr const char* kInstancePidFileName = "instance.pid";
constexpr const char* kInstancePortFileName = "instance.port";
constexpr const char* kInstanceStopRequestFileName = "instance.stop";

std::vector<Instance> instances;

using Command = std::function<void(std::stringstream&)>;

std::string removeQuotes(const std::string& s) {
    if (s.size() >= 2 && s.front() == '"' && s.back() == '"')
        return s.substr(1, s.size() - 2);

    return s;
}

std::string trim(const std::string& s) {
    std::size_t first = 0;

    while (first < s.size() && std::isspace(static_cast<unsigned char>(s[first])))
        ++first;

    std::size_t last = s.size();

    while (last > first && std::isspace(static_cast<unsigned char>(s[last - 1])))
        --last;

    return s.substr(first, last - first);
}

std::optional<std::uint16_t> tryParsePort(const std::string& value) {
    if (value.empty())
        return std::nullopt;

    std::uint64_t parsed = 0;

    for (const char ch : value) {
        if (!std::isdigit(static_cast<unsigned char>(ch)))
            return std::nullopt;

        parsed = parsed * 10 + static_cast<std::uint64_t>(ch - '0');

        if (parsed > std::numeric_limits<std::uint16_t>::max())
            return std::nullopt;
    }

    return static_cast<std::uint16_t>(parsed);
}

bool containsWhitespace(const std::string& s) {
    for (const char ch : s) {
        if (std::isspace(static_cast<unsigned char>(ch)))
            return true;
    }

    return false;
}

bool isInitCancelInput(const std::string& input) {
    const std::string value = trim(input);

    return value == "cancel"
        || value == ":cancel"
        || value == "abort"
        || value == ":abort";
}

std::optional<std::string> promptRequiredInitValue(const std::string& prompt) {
    while (true) {
        std::cout << prompt << " (cancel = abort init): " << std::flush;

        std::string line;

        if (!std::getline(std::cin, line))
            return std::nullopt;

        if (isInitCancelInput(line))
            return std::nullopt;

        std::string value = removeQuotes(trim(line));

        if (!value.empty())
            return value;

        std::cout << "Value is required. Type cancel to abort init.\n";
    }
}

std::optional<std::string> promptOptionalModuleImpl(
    const std::string& moduleName,
    const std::string& defaultImpl
) {
    std::cout << moduleName << " implementation [" << defaultImpl
        << "] (empty = default, cancel = abort init): " << std::flush;

    std::string line;

    if (!std::getline(std::cin, line))
        return std::nullopt;

    if (isInitCancelInput(line))
        return std::nullopt;

    std::string value = removeQuotes(trim(line));

    if (value.empty())
        return defaultImpl;

    return value;
}

std::string quote(const std::string& s) {
    if (s.size() >= 2 && s.front() == '"' && s.back() == '"')
        return s;

    return "\"" + s + "\"";
}

fs::path getSourceRoot() {
    return fs::path(KVDB_SOURCE_DIR);
}

fs::path getDistRoot() {
    return fs::path(KVDB_DIST_DIR);
}

fs::path getInstancesRoot() {
    return fs::path(KVDB_INSTANCES_ROOT_DIR);
}

fs::path getInstancesRegistryPath() {
    return getInstancesRoot() / kInstancesRegistryFileName;
}

fs::path getLegacyInstancesRegistryPath() {
    return getDistRoot() / kInstancesRegistryFileName;
}

fs::path getInstanceDir(const Instance& instance) {
    return fs::path(instance.path) / instance.name;
}

fs::path getInstancePidPath(const Instance& instance) {
    return getInstanceDir(instance) / kInstancePidFileName;
}

fs::path getInstancePortPath(const Instance& instance) {
    return getInstanceDir(instance) / kInstancePortFileName;
}

fs::path getInstanceStopRequestPath(const Instance& instance) {
    return getInstanceDir(instance) / kInstanceStopRequestFileName;
}

bool writeStopRequestFile(const Instance& instance) {
    const fs::path stopPath = getInstanceStopRequestPath(instance);

    std::ofstream output(stopPath);

    if (!output.is_open()) {
        std::cout << "Failed to create stop request file: " << stopPath << "\n";
        return false;
    }

    output << "stop\n";
    return true;
}


std::string buildModuleFileName(const std::string& moduleName, const std::string& implName) {
    return moduleName + "_" + implName + kModuleExtension;
}

fs::path getBuiltModulePath(const std::string& moduleName, const std::string& implName) {
    return getDistRoot() / "modules" / buildModuleFileName(moduleName, implName);
}

fs::path getBuiltDbInstancePath() {
    return getDistRoot() / kDbInstanceFileName;
}

const Instance* findInstanceByName(const std::string& name) {
    for (const auto& instance : instances) {
        if (instance.name == name)
            return &instance;
    }

    return nullptr;
}

bool ensureFileExists(const fs::path& path, const std::string& description) {
    if (fs::exists(path))
        return true;

    std::cout << description << " not found: " << path << "\n";
    return false;
}

bool copyFileChecked(const fs::path& source, const fs::path& destination) {
    try {
        fs::copy_file(source, destination, fs::copy_options::overwrite_existing);
        return true;
    }
    catch (const std::exception& ex) {
        std::cout << "Failed to copy file from " << source << " to " << destination
            << ". " << ex.what() << "\n";
        return false;
    }
}

bool hasRegisteredInstanceName(const std::string& name) {
    return findInstanceByName(name) != nullptr;
}

bool containsString(const std::vector<std::string>& values, const std::string& value) {
    for (const auto& item : values) {
        if (item == value)
            return true;
    }

    return false;
}

bool looksLikeInstanceDirectory(const fs::path& instanceDir) {
    std::error_code ec;

    if (!fs::is_directory(instanceDir, ec))
        return false;

    return fs::exists(instanceDir / kInstanceSettingsFileName, ec)
        && fs::exists(instanceDir / kDbInstanceFileName, ec);
}

bool registerExistingInstanceDirectory(const fs::path& instanceDir) {
    if (!looksLikeInstanceDirectory(instanceDir))
        return false;

    const std::string name = instanceDir.filename().string();
    const std::string parentPath = instanceDir.parent_path().string();

    if (name.empty() || parentPath.empty())
        return false;

    if (hasRegisteredInstanceName(name))
        return false;

    instances.push_back({name, parentPath});
    return true;
}

void discoverInstancesFromRoot(const fs::path& rootPath) {
    std::error_code ec;

    if (!fs::is_directory(rootPath, ec))
        return;

    fs::directory_iterator it(rootPath, ec);

    if (ec) {
        std::cout << "Failed to scan instances root: " << rootPath
            << ". " << ec.message() << "\n";
        return;
    }

    for (const auto& entry : it) {
        if (entry.is_directory(ec))
            registerExistingInstanceDirectory(entry.path());
    }
}

bool saveInstancesRegistry() {
    const fs::path registryPath = getInstancesRegistryPath();

    try {
        const fs::path parentPath = registryPath.parent_path();

        if (!parentPath.empty())
            fs::create_directories(parentPath);
    }
    catch (const std::exception& ex) {
        std::cout << "Failed to create instances registry directory: " << ex.what() << "\n";
        return false;
    }

    std::ofstream output(registryPath);

    if (!output.is_open()) {
        std::cout << "Failed to create instances registry: " << registryPath << "\n";
        return false;
    }

    for (const auto& instance : instances)
        output << instance.name << '\t' << instance.path << '\n';

    return true;
}

void loadInstancesRegistryFromFile(const fs::path& registryPath) {
    if (!fs::exists(registryPath))
        return;

    std::ifstream input(registryPath);

    if (!input.is_open()) {
        std::cout << "Failed to open instances registry: " << registryPath << "\n";
        return;
    }

    std::string line;

    while (std::getline(input, line)) {
        if (line.empty())
            continue;

        std::stringstream lineStream(line);
        std::string name;
        std::string path;

        std::getline(lineStream, name, '\t');
        std::getline(lineStream, path);

        name = trim(name);
        path = trim(path);

        if (name.empty() || path.empty())
            continue;

        if (hasRegisteredInstanceName(name))
            continue;

        const fs::path instanceDir = fs::path(path) / name;

        if (!looksLikeInstanceDirectory(instanceDir))
            continue;

        instances.push_back({name, path});
    }
}

void loadInstancesRegistry() {
    const std::size_t beforeLoadCount = instances.size();

    loadInstancesRegistryFromFile(getInstancesRegistryPath());

    const fs::path legacyRegistryPath = getLegacyInstancesRegistryPath();
    if (legacyRegistryPath != getInstancesRegistryPath())
        loadInstancesRegistryFromFile(legacyRegistryPath);

    discoverInstancesFromRoot(getInstancesRoot());

    if (instances.size() != beforeLoadCount)
        saveInstancesRegistry();
}

bool writeInstanceSettings(
    const fs::path& instanceDir,
    const std::string& engineDllFileName,
    const std::string& parserDllFileName,
    const std::string& accessInterfaceDllFileName,
    const std::string& responseDllFileName
) {
    const fs::path settingsPath = instanceDir / kInstanceSettingsFileName;
    std::ofstream output(settingsPath);

    if (!output.is_open()) {
        std::cout << "Failed to create " << settingsPath << "\n";
        return false;
    }

    output << engineDllFileName << '\n';
    output << parserDllFileName << '\n';
    output << accessInterfaceDllFileName << '\n';
    output << responseDllFileName << '\n';

    return true;
}

#ifdef _WIN32
bool copyFirstExistingRuntimeDll(
    const fs::path& instanceDir,
    const std::vector<std::string>& possibleFileNames,
    const std::string& description
) {
    for (const auto& fileName : possibleFileNames) {
        const fs::path source = getDistRoot() / fileName;

        if (!fs::exists(source))
            continue;

        const fs::path destination = instanceDir / fileName;
        return copyFileChecked(source, destination);
    }

    std::cout << description << " not found in dist. Tried:\n";

    for (const auto& fileName : possibleFileNames)
        std::cout << "  " << (getDistRoot() / fileName) << "\n";

    return false;
}

bool copyRuntimeDllsToInstance(const fs::path& instanceDir) {
    bool copiedAll = true;

    copiedAll = copyFirstExistingRuntimeDll(
        instanceDir,
        {"libstdc++-6.dll"},
        "C++ runtime DLL"
    ) && copiedAll;

    copiedAll = copyFirstExistingRuntimeDll(
        instanceDir,
        {"libwinpthread-1.dll"},
        "WinPthread runtime DLL"
    ) && copiedAll;

    copiedAll = copyFirstExistingRuntimeDll(
        instanceDir,
        {
            "libgcc_s_seh-1.dll",
            "libgcc_s_dw2-1.dll",
            "libgcc_s_sjlj-1.dll"
        },
        "GCC runtime DLL"
    ) && copiedAll;

    return copiedAll;
}
#endif

bool createInstance(
    const std::string& path,
    const std::string& name,
    const std::string& parserImpl,
    const std::string& engineImpl,
    const std::string& responseImpl,
    const std::string& accessInterfaceImpl
) {
    const fs::path instanceDir = fs::path(path) / name;

    if (hasRegisteredInstanceName(name)) {
        std::cout << "Instance with this name is already registered: " << name << "\n";
        return false;
    }

    if (fs::exists(instanceDir)) {
        if (registerExistingInstanceDirectory(instanceDir)) {
            if (!saveInstancesRegistry())
                std::cout << "Warning: existing instance was registered, but registry was not saved\n";

            std::cout << "Instance already exists on disk and was registered: "
                << instanceDir << "\n";
            return true;
        }

        std::cout << "Path already exists but is not a kvdb instance: "
            << instanceDir << "\n";
        return false;
    }

    const fs::path builtExecPath = getBuiltDbInstancePath();

    const fs::path builtEngineDllPath = getBuiltModulePath("engine", engineImpl);
    const fs::path builtParserDllPath = getBuiltModulePath("query_parser", parserImpl);
    const fs::path builtAccessInterfaceDllPath = getBuiltModulePath("access_interface", accessInterfaceImpl);
    const fs::path builtResponseDllPath = getBuiltModulePath("response_constructor", responseImpl);

    if (!ensureFileExists(builtExecPath, "db_instance executable"))
        return false;

    if (!ensureFileExists(builtEngineDllPath, "engine module"))
        return false;

    if (!ensureFileExists(builtParserDllPath, "query_parser module"))
        return false;

    if (!ensureFileExists(builtAccessInterfaceDllPath, "access_interface module"))
        return false;

    if (!ensureFileExists(builtResponseDllPath, "response_constructor module"))
        return false;

#ifdef _WIN32
    if (!ensureFileExists(getDistRoot() / "libstdc++-6.dll", "C++ runtime DLL"))
        return false;

    if (!ensureFileExists(getDistRoot() / "libwinpthread-1.dll", "WinPthread runtime DLL"))
        return false;
#endif

    try {
        fs::create_directories(instanceDir);
    }
    catch (const std::exception& ex) {
        std::cout << "Failed to create instance directory: " << ex.what() << "\n";
        return false;
    }

    const fs::path instanceExecPath = instanceDir / kDbInstanceFileName;

    const std::string engineDllFileName = builtEngineDllPath.filename().string();
    const std::string parserDllFileName = builtParserDllPath.filename().string();
    const std::string accessInterfaceDllFileName = builtAccessInterfaceDllPath.filename().string();
    const std::string responseDllFileName = builtResponseDllPath.filename().string();

    if (!copyFileChecked(builtExecPath, instanceExecPath))
        return false;

#ifdef _WIN32
    if (!copyRuntimeDllsToInstance(instanceDir))
        return false;
#endif

    if (!copyFileChecked(builtEngineDllPath, instanceDir / engineDllFileName))
        return false;

    if (!copyFileChecked(builtParserDllPath, instanceDir / parserDllFileName))
        return false;

    if (!copyFileChecked(builtAccessInterfaceDllPath, instanceDir / accessInterfaceDllFileName))
        return false;

    if (!copyFileChecked(builtResponseDllPath, instanceDir / responseDllFileName))
        return false;

    if (!writeInstanceSettings(
        instanceDir,
        engineDllFileName,
        parserDllFileName,
        accessInterfaceDllFileName,
        responseDllFileName))
        return false;

    instances.push_back({name, path});

    if (!saveInstancesRegistry())
        std::cout << "Warning: instance was created, but registry was not saved\n";

    return true;
}

std::optional<std::uint64_t> readInstancePid(const Instance& instance) {
    const fs::path pidPath = getInstancePidPath(instance);

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

std::optional<std::uint16_t> readInstancePort(const Instance& instance) {
    const fs::path portPath = getInstancePortPath(instance);

    if (!fs::exists(portPath))
        return std::nullopt;

    std::ifstream input(portPath);

    if (!input.is_open())
        return std::nullopt;

    std::string rawPort;
    input >> rawPort;

    return tryParsePort(rawPort);
}

void removeInstancePidFile(const Instance& instance) {
    std::error_code ec;
    fs::remove(getInstancePidPath(instance), ec);
}

void removeInstancePortFile(const Instance& instance) {
    std::error_code ec;
    fs::remove(getInstancePortPath(instance), ec);
}

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

    // Important for Docker/Linux:
    // kill(pid, 0) returns success for zombie children too.
    // A background db_instance is a child of the manager, so we must reap it
    // with waitpid(..., WNOHANG). Otherwise show/stop can keep treating an
    // already-finished instance as running forever.
    while (true) {
        int status = 0;
        const pid_t waitResult = waitpid(nativePid, &status, WNOHANG);

        if (waitResult == nativePid) {
            return false;
        }

        if (waitResult == 0) {
            return true;
        }

        if (errno == EINTR) {
            continue;
        }

        break;
    }

    if (kill(nativePid, 0) == 0)
        return true;

    return errno == EPERM;
#endif
}

InstanceRunState getInstanceRunState(const Instance& instance) {
    const std::optional<std::uint64_t> pid = readInstancePid(instance);

    if (!pid.has_value())
        return {};

    if (isProcessRunning(*pid))
        return {true, pid, readInstancePort(instance)};

    removeInstancePidFile(instance);
    removeInstancePortFile(instance);
    return {};
}

bool waitForInstanceStop(
    const Instance& instance,
    const std::chrono::milliseconds timeout
) {
    const auto startedAt = std::chrono::steady_clock::now();

    while (std::chrono::steady_clock::now() - startedAt < timeout) {
        const InstanceRunState state = getInstanceRunState(instance);

        if (!state.isRunning)
            return true;

        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    return false;
}

void stopInstance(const std::string& name) {
    const Instance* instance = findInstanceByName(name);

    if (instance == nullptr) {
        std::cout << "Instance not found\n";
        return;
    }

    const InstanceRunState state = getInstanceRunState(*instance);

    if (!state.isRunning) {
        std::cout << "Instance is not running\n";
        return;
    }

    if (!writeStopRequestFile(*instance))
        return;

    std::cout << "Stop requested";

    if (state.pid.has_value())
        std::cout << " for PID " << *state.pid;

    std::cout << "\n";

    if (waitForInstanceStop(*instance, std::chrono::seconds(5))) {
        std::cout << "Instance stopped gracefully\n";
        return;
    }

    std::cout << "Instance did not stop within timeout.\n";
    std::cout << "Do not force-kill it if you need engine.onInstanceShutdown() to run.\n";
}

Command constructStopCommand() {
    return [](std::stringstream& ss) {
        std::string name;

        if (!(ss >> name)) {
            std::cout << "Usage: stop <name>\n";
            return;
        }

        std::string extraArg;

        if (ss >> extraArg) {
            std::cout << "Unknown stop argument: " << extraArg << "\n";
            return;
        }

        name = removeQuotes(name);
        stopInstance(name);
    };
}

std::vector<std::string> buildDbInstanceArgs(const std::optional<std::uint16_t> port) {
    std::vector<std::string> args;

    if (port.has_value()) {
        args.emplace_back("--port");
        args.push_back(std::to_string(*port));
    }

    return args;
}

std::string buildProcessCommandLine(
    const fs::path& executablePath,
    const std::optional<std::uint16_t> port
) {
    std::string commandLine = quote(executablePath.string());

    for (const auto& arg : buildDbInstanceArgs(port)) {
        commandLine += ' ';
        commandLine += quote(arg);
    }

    return commandLine;
}

#ifndef _WIN32
[[noreturn]]
void execDbInstance(
    const fs::path& executablePath,
    const std::optional<std::uint16_t> port
) {
    std::vector<std::string> argValues;
    argValues.push_back(executablePath.filename().string());

    for (auto& arg : buildDbInstanceArgs(port))
        argValues.push_back(std::move(arg));

    std::vector<char*> argv;
    argv.reserve(argValues.size() + 1);

    for (auto& arg : argValues)
        argv.push_back(arg.data());

    argv.push_back(nullptr);

    execv(executablePath.string().c_str(), argv.data());
    _exit(127);
}
#endif

bool startProcessForeground(
    const fs::path& executablePath,
    const fs::path& workingDirectory,
    const std::optional<std::uint16_t> port
) {
#ifdef _WIN32
    STARTUPINFOA si{};
    PROCESS_INFORMATION pi{};

    si.cb = sizeof(si);

    std::string cmdLine = buildProcessCommandLine(executablePath, port);
    std::vector<char> mutableCmdLine(cmdLine.begin(), cmdLine.end());
    mutableCmdLine.push_back('\0');

    const std::string workDir = workingDirectory.string();
    const char* workDirPtr = workDir.empty() ? nullptr : workDir.c_str();

    if (!CreateProcessA(
        nullptr,
        mutableCmdLine.data(),
        nullptr,
        nullptr,
        FALSE,
        0,
        nullptr,
        workDirPtr,
        &si,
        &pi)) {
        std::cout << "Failed to start process. Windows error: " << GetLastError() << "\n";
        return false;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    if (exitCode != 0) {
        std::cout << "Process exited with code: "
            << static_cast<std::int32_t>(exitCode) << "\n";
        return false;
    }

    return true;
#else
    const pid_t childPid = fork();

    if (childPid < 0) {
        std::cout << "Failed to fork process\n";
        return false;
    }

    if (childPid == 0) {
        if (!workingDirectory.empty())
            chdir(workingDirectory.string().c_str());

        execDbInstance(executablePath, port);
    }

    int status = 0;

    if (waitpid(childPid, &status, 0) < 0) {
        std::cout << "Failed to wait for process\n";
        return false;
    }

    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        std::cout << "Process exited with error\n";
        return false;
    }

    return true;
#endif
}

bool startProcessBackground(
    const fs::path& executablePath,
    const fs::path& workingDirectory,
    const std::optional<std::uint16_t> port,
    std::uint64_t& pid
) {
#ifdef _WIN32
    STARTUPINFOA si{};
    PROCESS_INFORMATION pi{};

    si.cb = sizeof(si);

    std::string cmdLine = buildProcessCommandLine(executablePath, port);
    std::vector<char> mutableCmdLine(cmdLine.begin(), cmdLine.end());
    mutableCmdLine.push_back('\0');

    const std::string workDir = workingDirectory.string();
    const char* workDirPtr = workDir.empty() ? nullptr : workDir.c_str();

    if (!CreateProcessA(
        nullptr,
        mutableCmdLine.data(),
        nullptr,
        nullptr,
        FALSE,
        CREATE_NEW_CONSOLE,
        nullptr,
        workDirPtr,
        &si,
        &pi)) {
        std::cout << "Failed to start process in background. Windows error: "
            << GetLastError() << "\n";
        return false;
    }

    pid = static_cast<std::uint64_t>(pi.dwProcessId);

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    return true;
#else
    const pid_t childPid = fork();

    if (childPid < 0) {
        std::cout << "Failed to fork background process\n";
        return false;
    }

    if (childPid == 0) {
        if (!workingDirectory.empty())
            chdir(workingDirectory.string().c_str());

        execDbInstance(executablePath, port);
    }

    pid = static_cast<std::uint64_t>(childPid);
    return true;
#endif
}

bool waitForInstanceReady(
    const Instance& instance,
    const std::chrono::milliseconds timeout
) {
    const auto startedAt = std::chrono::steady_clock::now();

    while (std::chrono::steady_clock::now() - startedAt < timeout) {
        const InstanceRunState state = getInstanceRunState(instance);

        if (state.isRunning && state.port.has_value())
            return true;

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    return false;
}

void showInstances() {
    if (instances.empty()) {
        std::cout << "No instances registered\n";
        return;
    }

    std::cout
        << std::left
        << std::setw(18) << "NAME"
        << std::setw(12) << "STATUS"
        << std::setw(10) << "PID"
        << std::setw(8) << "PORT"
        << "DIRECTORY"
        << "\n";

    std::cout << std::string(70, '-') << "\n";

    for (const auto& instance : instances) {
        const InstanceRunState state = getInstanceRunState(instance);

        const std::string status = state.isRunning ? "running" : "stopped";
        const std::string pid = state.pid.has_value()
                                    ? std::to_string(*state.pid)
                                    : "-";

        const std::string port = state.port.has_value()
                                     ? std::to_string(*state.port)
                                     : "-";

        std::cout
            << std::left
            << std::setw(18) << instance.name
            << std::setw(12) << status
            << std::setw(10) << pid
            << std::setw(8) << port
            << getInstanceDir(instance).string()
            << "\n";
    }
}

void runInstance(
    const std::string& name,
    const bool background,
    const std::optional<std::uint16_t> port
) {
    const Instance* instance = findInstanceByName(name);

    if (instance == nullptr) {
        std::cout << "Instance not found\n";
        return;
    }

    const InstanceRunState state = getInstanceRunState(*instance);

    if (state.isRunning) {
        std::cout << "Instance is already running";

        if (state.pid.has_value())
            std::cout << " with PID " << *state.pid;

        if (state.port.has_value())
            std::cout << " on port " << *state.port;

        std::cout << "\n";
        return;
    }

    const fs::path instanceDir = getInstanceDir(*instance);
    const fs::path execPath = instanceDir / kDbInstanceFileName;

    if (!fs::exists(execPath)) {
        std::cout << "Instance executable not found: " << execPath << "\n";
        return;
    }

    if (background) {
        std::uint64_t processPid = 0;

        if (!startProcessBackground(execPath, instanceDir, port, processPid)) {
            std::cout << "Instance was not started\n";
            return;
        }

        if (waitForInstanceReady(*instance, std::chrono::milliseconds(3000))) {
            const InstanceRunState registeredState = getInstanceRunState(*instance);

            std::cout << "Instance started in background";

            if (registeredState.pid.has_value())
                std::cout << ". PID: " << *registeredState.pid;
            else
                std::cout << ". Process PID: " << processPid;

            if (registeredState.port.has_value())
                std::cout << ". Port: " << *registeredState.port;

            std::cout << "\n";
            return;
        }

        const InstanceRunState currentState = getInstanceRunState(*instance);

        std::cout << "Process started in background. Process PID: " << processPid << "\n";

        if (currentState.isRunning && !currentState.port.has_value()) {
            std::cout << "Warning: instance.port was not created yet. Use show in a moment.\n";
        }
        else {
            std::cout << "Warning: instance.pid was not created yet. Use show in a moment.\n";
        }

        return;
    }

    std::cout << "Instance started in foreground\n";

    const bool ok = startProcessForeground(execPath, instanceDir, port);

    if (!ok)
        std::cout << "Instance exited with error\n";
    else
        std::cout << "Instance stopped\n";
}

void addModuleImplIfNotExists(
    std::vector<std::string>& implNames,
    const std::string& implName
) {
    if (!implName.empty() && !containsString(implNames, implName))
        implNames.push_back(implName);
}

void collectBuiltModuleImpls(
    const std::string& moduleName,
    std::vector<std::string>& implNames
) {
    const fs::path builtModulesRoot = getDistRoot() / "modules";

    std::error_code ec;
    if (!fs::is_directory(builtModulesRoot, ec))
        return;

    const std::string prefix = moduleName + "_";
    const std::string suffix = kModuleExtension;

    fs::directory_iterator it(builtModulesRoot, ec);

    if (ec) {
        std::cout << "Failed to scan built modules directory: "
            << builtModulesRoot << ". " << ec.message() << "\n";
        return;
    }

    for (const auto& entry : it) {
        if (!entry.is_regular_file(ec))
            continue;

        const std::string fileName = entry.path().filename().string();

        if (!fileName.starts_with(prefix) || !fileName.ends_with(suffix))
            continue;

        const std::string implName = fileName.substr(
            prefix.size(),
            fileName.size() - prefix.size() - suffix.size()
        );

        addModuleImplIfNotExists(implNames, implName);
    }
}

void collectSourceModuleImpls(
    const std::string& moduleName,
    std::vector<std::string>& implNames
) {
    const fs::path moduleRoot = getSourceRoot() / "modules" / moduleName;

    std::error_code ec;
    if (!fs::is_directory(moduleRoot, ec))
        return;

    fs::directory_iterator it(moduleRoot, ec);

    if (ec) {
        std::cout << "Failed to scan source module directory: "
            << moduleRoot << ". " << ec.message() << "\n";
        return;
    }

    for (const auto& entry : it) {
        if (entry.is_directory(ec))
            addModuleImplIfNotExists(implNames, entry.path().filename().string());
    }
}

void listModules(const std::string& moduleName) {
    std::vector<std::string> implNames;

    collectBuiltModuleImpls(moduleName, implNames);
    collectSourceModuleImpls(moduleName, implNames);

    std::cout << moduleName << ":\n";

    if (implNames.empty()) {
        std::cout << "  (none)\n";
        return;
    }

    for (const auto& implName : implNames)
        std::cout << "  " << implName << '\n';
}

void modulesCommand() {
    listModules("query_parser");
    listModules("engine");
    listModules("response_constructor");
    listModules("access_interface");
}

void printRunUsage() {
    std::cout << "Usage: run [-b|--background] <name> [--port <port>|--port=<port>]\n";
}

void helpCommand() {
    std::cout << "Available commands:\n";
    std::cout << "  init\n";
    std::cout << "      Start interactive instance creation wizard.\n";
    std::cout << "      Inside init, type cancel, :cancel, abort, or :abort to leave safely.\n";
    std::cout << "  show\n";
    std::cout << "      Show registered instances and their runtime status.\n";
    std::cout << "  run [-b|--background] <name> [--port <port>|--port=<port>]\n";
    std::cout << "      Run registered instance. Without -b it runs in foreground; with -b it runs in background.\n";
    std::cout << "  stop <name>\n";
    std::cout << "      Gracefully stop running instance.\n";
    std::cout << "  modules\n";
    std::cout << "      Show available module implementations.\n";
    std::cout << "  help\n";
    std::cout << "      Show this help.\n";
    std::cout << "  exit\n";
    std::cout << "      Exit manager.\n";
}

Command constructInitCommand() {
    return [](std::stringstream& ss) {
        std::string unexpectedArg;

        if (ss >> unexpectedArg) {
            std::cout << "init does not accept command-line arguments anymore.\n";
            std::cout << "Use just: init\n";
            return;
        }

        std::cout << "Starting interactive init.\n";
        std::cout << "Type cancel at any step to abort without creating an instance.\n";

        auto pathInput = promptRequiredInitValue("Instance parent path");

        if (!pathInput.has_value()) {
            std::cout << "Init cancelled\n";
            return;
        }

        const std::string path = *pathInput;

        if (!fs::exists(path)) {
            std::cout << "Path does not exist\n";
            std::cout << "Init cancelled\n";
            return;
        }

        auto nameInput = promptRequiredInitValue("Instance name");

        if (!nameInput.has_value()) {
            std::cout << "Init cancelled\n";
            return;
        }

        const std::string name = *nameInput;

        if (containsWhitespace(name)) {
            std::cout << "Instance name cannot contain whitespace.\n";
            std::cout << "Init cancelled\n";
            return;
        }

        std::cout << "Choose module implementations. Empty input means standard.\n";

        auto parserInput = promptOptionalModuleImpl("query_parser", "standard");

        if (!parserInput.has_value()) {
            std::cout << "Init cancelled\n";
            return;
        }

        auto engineInput = promptOptionalModuleImpl("engine", "standard");

        if (!engineInput.has_value()) {
            std::cout << "Init cancelled\n";
            return;
        }

        auto responseInput = promptOptionalModuleImpl("response_constructor", "standard");

        if (!responseInput.has_value()) {
            std::cout << "Init cancelled\n";
            return;
        }

        auto accessInterfaceInput = promptOptionalModuleImpl("access_interface", "standard");

        if (!accessInterfaceInput.has_value()) {
            std::cout << "Init cancelled\n";
            return;
        }

        if (createInstance(
            path,
            name,
            *parserInput,
            *engineInput,
            *responseInput,
            *accessInterfaceInput)) {
            std::cout << "Instance created\n";
        }
    };
}

Command constructShowCommand() {
    return [](std::stringstream&) {
        showInstances();
    };
}

Command constructRunCommand() {
    return [](std::stringstream& ss) {
        bool background = false;
        std::optional<std::uint16_t> port;
        std::string name;
        std::string arg;

        while (ss >> arg) {
            if (arg == "-b" || arg == "--background") {
                background = true;
                continue;
            }

            if (arg == "-p" || arg == "--port") {
                std::string portText;

                if (!(ss >> portText)) {
                    std::cout << "Expected port after " << arg << "\n";
                    return;
                }

                auto parsedPort = tryParsePort(removeQuotes(portText));

                if (!parsedPort.has_value()) {
                    std::cout << "Port must be a number from 0 to 65535\n";
                    return;
                }

                port = parsedPort;
                continue;
            }

            if (arg.starts_with("--port=")) {
                auto parsedPort = tryParsePort(arg.substr(7));

                if (!parsedPort.has_value()) {
                    std::cout << "Port must be a number from 0 to 65535\n";
                    return;
                }

                port = parsedPort;
                continue;
            }

            if (name.empty()) {
                name = removeQuotes(arg);
                continue;
            }

            if (!port.has_value()) {
                auto parsedPort = tryParsePort(removeQuotes(arg));

                if (parsedPort.has_value()) {
                    port = parsedPort;
                    continue;
                }
            }

            std::cout << "Unknown run argument: " << arg << "\n";
            return;
        }

        if (name.empty()) {
            printRunUsage();
            return;
        }

        runInstance(name, background, port);
    };
}

Command constructModulesCommand() {
    return [](std::stringstream&) {
        modulesCommand();
    };
}

Command constructHelpCommand() {
    return [](std::stringstream&) {
        helpCommand();
    };
}

std::unordered_map<std::string, Command> registerCommands() {
    std::unordered_map<std::string, Command> commands;

    commands["init"] = constructInitCommand();
    commands["show"] = constructShowCommand();
    commands["run"] = constructRunCommand();
    commands["modules"] = constructModulesCommand();
    commands["help"] = constructHelpCommand();
    commands["stop"] = constructStopCommand();

    return commands;
}

int main() {
    loadInstancesRegistry();

    auto commands = registerCommands();

    std::string line;

    while (true) {
        std::cout << "kvdb> " << std::flush;

        if (!std::getline(std::cin, line))
            break;

        std::stringstream ss(line);

        std::string command;
        ss >> command;

        if (command == "exit")
            break;

        if (command.empty())
            continue;

        const auto it = commands.find(command);

        if (it != commands.end())
            it->second(ss);
        else
            std::cout << "Unknown command\n";
    }

    return 0;
}
