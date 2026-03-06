#include <iostream>
#include <filesystem>
#include <string>
#include <vector>
#include <sstream>
#include <unordered_map>
#include <functional>

namespace fs = std::filesystem;

#ifndef KVDB_CMAKE_PATH
#define KVDB_CMAKE_PATH "cmake"
#endif

#ifndef KVDB_SOURCE_DIR
#define KVDB_SOURCE_DIR "."
#endif

struct Instance {
    std::string name;
    std::string path;
};
#ifdef _WIN32
#include <windows.h>
#endif

bool runProcess(const std::string &command) {
#ifdef _WIN32

    STARTUPINFOA si{};
    PROCESS_INFORMATION pi{};

    si.cb = sizeof(si);

    std::string cmd = command;

    if (!CreateProcessA(
        NULL,
        cmd.data(),
        NULL,
        NULL,
        FALSE,
        0,
        NULL,
        NULL,
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

    return system(command.c_str()) == 0;

#endif
}

std::vector<Instance> instances;

using Command = std::function<void(std::stringstream &)>;

std::string removeQuotes(const std::string &s) {
    if (s.size() >= 2 && s.front() == '"' && s.back() == '"')
        return s.substr(1, s.size() - 2);
    return s;
}

std::string quote(const std::string &s) {
    if (s.size() >= 2 && s.front() == '"' && s.back() == '"')
        return s;

    return "\"" + s + "\"";
}

std::vector<std::string> findCppFiles(const fs::path &dir) {
    std::vector<std::string> result;

    for (auto &p: fs::directory_iterator(dir)) {
        if (p.path().extension() == ".cpp")
            result.push_back(quote(p.path().generic_string()));
    }

    return result;
}

bool createInstance(
    const std::string &path,
    const std::string &name,
    const std::string &parser,
    const std::string &engine,
    const std::string &response,
    const std::string &server
) {
    std::string instanceDir = path + "/" + name;

    if (fs::exists(instanceDir)) {
        std::cout << "Instance already exists: " << name << "\n";
        return false;
    }

    try {
        fs::create_directories(instanceDir);
    } catch (...) {
        std::cout << "Failed to create directory\n";
        return false;
    }

    fs::path serverProject = fs::path(KVDB_SOURCE_DIR).parent_path() / "server_project";

    std::vector<std::string> sources;

    auto addSources = [&](const fs::path &dir) {
        auto files = findCppFiles(dir);
        sources.insert(sources.end(), files.begin(), files.end());
    };

    addSources(serverProject / "server/impl" / server);
    addSources(serverProject / "query_parser/impl" / parser);
    addSources(serverProject / "engine/impl" / engine);
    addSources(serverProject / "response_constructor/impl" / response);

    std::string cmd = "g++ -std=c++20 ";

    for (auto &s: sources)
        cmd += s + " ";

    cmd += "-I" + quote((serverProject / "contracts").generic_string()) + " ";
    cmd += "-I" + quote((serverProject / "query_parser").generic_string()) + " ";
    cmd += "-I" + quote((serverProject / "engine").generic_string()) + " ";
    cmd += "-I" + quote((serverProject / "response_constructor").generic_string()) + " ";

    cmd += "-o " + quote(instanceDir + "/server.exe");
    std::cout << cmd << std::endl;

    if (!runProcess(cmd)) {
        std::cout << "Compilation failed\n";
        return false;
    }

    instances.push_back({name, path});
    return true;
}

void showInstances() {
    if (instances.empty()) {
        std::cout << "No instances created\n";
        return;
    }

    for (const auto &i: instances)
        std::cout << i.name << " - " << i.path << std::endl;
}

void runInstance(const std::string &name) {
    for (const auto &i: instances) {
        if (i.name == name) {
            std::string execPath = i.path + "/" + name + "/server.exe";

            system(execPath.c_str());
            return;
        }
    }

    std::cout << "Instance not found\n";
}

void listModules(const std::string &moduleName) {
    fs::path root = KVDB_SOURCE_DIR;
    fs::path implPath = root / moduleName / "impl";

    std::cout << moduleName << ":\n";

    if (!fs::exists(implPath)) {
        std::cout << "  (none)\n";
        return;
    }

    for (auto &p: fs::directory_iterator(implPath)) {
        if (p.is_directory())
            std::cout << "  " << p.path().filename().string() << "\n";
    }
}

void modulesCommand() {
    listModules("query_parser");
    listModules("engine");
    listModules("response_constructor");
    listModules("server");
}

Command constructInitCommand() {
    return [](std::stringstream &ss) {
        std::string path, name;

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
    return [](std::stringstream &) {
        showInstances();
    };
}

Command constructRunCommand() {
    return [](std::stringstream &ss) {
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
    return [](std::stringstream &) {
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

        auto it = commands.find(command);

        if (it != commands.end())
            it->second(ss);
        else
            std::cout << "Unknown command\n";
    }

    return 0;
}
