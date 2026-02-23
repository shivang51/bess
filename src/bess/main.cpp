#include "application.h"
#include "args_parser.h"
#include "crash_detail_helper.h"
#include <filesystem>
#include <iostream>

bool isValidStartDir();
bool validateStartDir(char **argv);
Bess::AppStartupFlags prepareFlags(const AppArgs &args);

int main(int argc, char **argv) {
    if (!validateStartDir(argv)) {
        std::cerr << "[-] Wrong working directory. Expected 'assets/' folder. Exiting." << std::endl;
        return -1;
    }

    SETUP_CRASH_DETAIL_HELPER

    AppArgs args;
    parseArgs(argc, argv, args);
    auto flags = prepareFlags(args);

    Bess::Application app;

    try {
        app.init(args.projectFile, flags);
        app.run();
        app.shutdown();
    } catch (const std::exception &e) {
        std::cerr << e.what() << std::endl;
#ifdef DEBUG
        const std::stacktrace st = std::stacktrace::current();
        std::cerr << st << std::endl;
#endif
        throw e;
        return -1;
    }

    return 0;
}

Bess::AppStartupFlags prepareFlags(const AppArgs &args) {
    Bess::AppStartupFlags flags = Bess::AppStartupFlag::none;
    if (args.disablePlugins) {
        flags |= Bess::AppStartupFlag::disablePlugins;
    }
    return flags;
}

bool isValidStartDir() {
    return std::filesystem::exists("assets");
}

bool validateStartDir(char **argv) {
    if (isValidStartDir()) {
        return true;
    }

    const std::filesystem::path exePath = std::filesystem::absolute(argv[0]);
    const std::filesystem::path exeDir = exePath.parent_path();
    std::filesystem::current_path(exeDir);

    return isValidStartDir();
}
