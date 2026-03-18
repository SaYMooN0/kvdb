#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

namespace fs = std::filesystem;

#ifndef KVDB_SOURCE_DIR
#define KVDB_SOURCE_DIR "."
#endif

#ifndef KVDB_DIST_DIR
#define KVDB_DIST_DIR "./dist"
#endif

struct Instance {
    std::string name;
    std::string path;
};

#ifdef _WIN32
constexpr const char* kServerInstanceExecFileName = "server_instance_exec.exe";
constexpr const char* kModuleExtension = ".dll";
#else
constexpr const char* kServerInstanceExecFileName = "server_instance_exec";
constexpr const char* kModuleExtension = ".so";
#endif

constexpr const char* kInstanceSettingsFileName = "instance_settings.txt";

std::vector<Instance> instances;

using Command = std::function<void(std::stringstream&)>;

std::string removeQuotes(const std::string& s) {
    if (s.size() >= 2 && s.front() == '"' && s.back() == '"')
        return s.substr(1, s.size() - 2);

    return s;
}

std::string quote(const std::string& s) {
    if (s.size() >= 2 && s.front() == '"' && s.back() == '"')
        return s;

    return "\"" + s + "\"";
}

bool runProcess(const std::string& command, const std::string& workingDirectory = "") {
#ifdef _WIN32
    STARTUPINFOA si{};
    PROCESS_INFORMATION pi{};

    si.cb = sizeof(si);

    std::string cmdLine = command;
    std::vector<char> mutableCmdLine(cmdLine.begin(), cmdLine.end());
    mutableCmdLine.push_back('\0');

    const char* workDirPtr = workingDirectory.empty() ? nullptr : workingDirectory.c_str();

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
        std::cout << "Failed to start process\n";
        return false;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return exitCode == 0;
#else
    std::string finalCommand = command;

    if (!workingDirectory.empty())
        finalCommand = "cd " + quote(workingDirectory) + " && " + command;

    return system(finalCommand.c_str()) == 0;
#endif
}

fs::path getSourceRoot() {
    return fs::path(KVDB_SOURCE_DIR);
}

fs::path getDistRoot() {
    return fs::path(KVDB_DIST_DIR);
}

std::string buildModuleFileName(const std::string& moduleName, const std::string& implName) {
    return moduleName + "_" + implName + kModuleExtension;
}

fs::path getBuiltModulePath(const std::string& moduleName, const std::string& implName) {
    return getDistRoot() / "modules" / buildModuleFileName(moduleName, implName);
}

fs::path getBuiltServerInstanceExecPath() {
    return getDistRoot() / kServerInstanceExecFileName;
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

bool writeInstanceSettings(
    const fs::path& instanceDir,
    const std::string& engineDllFileName,
    const std::string& parserDllFileName,
    const std::string& serverDllFileName,
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
    output << serverDllFileName << '\n';
    output << responseDllFileName << '\n';

    return true;
}

bool createInstance(
    const std::string& path,
    const std::string& name,
    const std::string& parserImpl,
    const std::string& engineImpl,
    const std::string& responseImpl,
    const std::string& serverImpl
) {
    const fs::path instanceDir = fs::path(path) / name;

    if (fs::exists(instanceDir)) {
        std::cout << "Instance already exists: " << instanceDir << "\n";
        return false;
    }

    const fs::path builtExecPath = getBuiltServerInstanceExecPath();

    const fs::path builtEngineDllPath = getBuiltModulePath("engine", engineImpl);
    const fs::path builtParserDllPath = getBuiltModulePath("query_parser", parserImpl);
    const fs::path builtServerDllPath = getBuiltModulePath("server", serverImpl);
    const fs::path builtResponseDllPath = getBuiltModulePath("response_constructor", responseImpl);

    if (!ensureFileExists(builtExecPath, "server_instance_exec executable"))
        return false;

    if (!ensureFileExists(builtEngineDllPath, "engine module"))
        return false;

    if (!ensureFileExists(builtParserDllPath, "query_parser module"))
        return false;

    if (!ensureFileExists(builtServerDllPath, "server module"))
        return false;

    if (!ensureFileExists(builtResponseDllPath, "response_constructor module"))
        return false;

    try {
        fs::create_directories(instanceDir);
    }
    catch (const std::exception& ex) {
        std::cout << "Failed to create instance directory: " << ex.what() << "\n";
        return false;
    }

    const fs::path instanceExecPath = instanceDir / kServerInstanceExecFileName;

    const std::string engineDllFileName = builtEngineDllPath.filename().string();
    const std::string parserDllFileName = builtParserDllPath.filename().string();
    const std::string serverDllFileName = builtServerDllPath.filename().string();
    const std::string responseDllFileName = builtResponseDllPath.filename().string();

    if (!copyFileChecked(builtExecPath, instanceExecPath))
        return false;

    if (!copyFileChecked(builtEngineDllPath, instanceDir / engineDllFileName))
        return false;

    if (!copyFileChecked(builtParserDllPath, instanceDir / parserDllFileName))
        return false;

    if (!copyFileChecked(builtServerDllPath, instanceDir / serverDllFileName))
        return false;

    if (!copyFileChecked(builtResponseDllPath, instanceDir / responseDllFileName))
        return false;

    if (!writeInstanceSettings(
        instanceDir,
        engineDllFileName,
        parserDllFileName,
        serverDllFileName,
        responseDllFileName))
        return false;

    instances.push_back({name, path});
    return true;
}

void showInstances() {
    if (instances.empty()) {
        std::cout << "No instances created in this manager session\n";
        return;
    }

    for (const auto& i : instances)
        std::cout << i.name << " - " << i.path << '\n';
}

void runInstance(const std::string& name) {
    for (const auto& i : instances) {
        if (i.name == name) {
            const fs::path instanceDir = fs::path(i.path) / i.name;
            const fs::path execPath = instanceDir / kServerInstanceExecFileName;

            if (!fs::exists(execPath)) {
                std::cout << "Instance executable not found: " << execPath << "\n";
                return;
            }

            const bool ok = runProcess(quote(execPath.string()), instanceDir.string());

            if (!ok)
                std::cout << "Instance exited with error\n";

            return;
        }
    }

    std::cout << "Instance not found\n";
}

void listModules(const std::string& moduleName) {
    const fs::path moduleRoot = getSourceRoot() / "modules" / moduleName;

    std::cout << moduleName << ":\n";

    if (!fs::exists(moduleRoot)) {
        std::cout << "  (none)\n";
        return;
    }

    bool foundAny = false;

    for (const auto& p : fs::directory_iterator(moduleRoot)) {
        if (p.is_directory()) {
            foundAny = true;
            std::cout << "  " << p.path().filename().string() << '\n';
        }
    }

    if (!foundAny)
        std::cout << "  (none)\n";
}

void modulesCommand() {
    listModules("query_parser");
    listModules("engine");
    listModules("response_constructor");
    listModules("server");
}

Command constructInitCommand() {
    return [](std::stringstream& ss) {
        std::string path;
        std::string name;

        if (!(ss >> path >> name)) {
            std::cout << "Usage:\n";
            std::cout << "init <path> <name> "
                         "[-q:parser] [-e:engine] [-r:response] [-s:server]\n";
            return;
        }

        path = removeQuotes(path);
        name = removeQuotes(name);

        if (!fs::exists(path)) {
            std::cout << "Path does not exist\n";
            return;
        }

        std::string parser = "standard";
        std::string engine = "standard";
        std::string response = "standard";
        std::string server = "standard";

        std::string arg;

        while (ss >> arg) {
            if (arg.starts_with("-q:"))
                parser = arg.substr(3);
            else if (arg.starts_with("-e:"))
                engine = arg.substr(3);
            else if (arg.starts_with("-r:"))
                response = arg.substr(3);
            else if (arg.starts_with("-s:"))
                server = arg.substr(3);
            else {
                std::cout << "Unknown argument: " << arg << "\n";
                return;
            }
        }

        if (createInstance(path, name, parser, engine, response, server))
            std::cout << "Instance created\n";
    };
}

Command constructShowCommand() {
    return [](std::stringstream&) {
        showInstances();
    };
}

Command constructRunCommand() {
    return [](std::stringstream& ss) {
        std::string name;

        if (!(ss >> name)) {
            std::cout << "Usage: run <name>\n";
            return;
        }

        name = removeQuotes(name);
        runInstance(name);
    };
}

Command constructModulesCommand() {
    return [](std::stringstream&) {
        modulesCommand();
    };
}

std::unordered_map<std::string, Command> registerCommands() {
    std::unordered_map<std::string, Command> commands;

    commands["init"] = constructInitCommand();
    commands["show"] = constructShowCommand();
    commands["run"] = constructRunCommand();
    commands["modules"] = constructModulesCommand();

    return commands;
}

int main() {
    auto commands = registerCommands();

    std::string line;

    while (true) {
        std::cout << "kvdb> ";
        std::getline(std::cin, line);

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