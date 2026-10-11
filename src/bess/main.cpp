#include "application.h"
#include "args_parser.h"
#include "common/bess_assert.h"
#include "crash_detail_helper.h"
#include <filesystem>
#include <iostream>

#ifdef __APPLE__
    #include <climits>
    #include <cstdint>
    #include <mach-o/dyld.h>
#endif

bool isValidStartDir();
bool validateStartDir(char **argv);
Bess::AppStartupFlags prepareFlags(const AppArgs &args);

int main(int argc, char **argv) {

    if (!validateStartDir(argv)) {
        std::cerr << "[-] Wrong working directory. Expected 'assets/' folder. "
                     "Exiting."
                  << std::endl;
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
    } catch (const std::exception &e) {
        std::cerr << e.what() << std::endl;
#ifdef DEBUG
        BESS_PRINT_STACKTRACE(std::cerr);
#endif
        throw;
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

std::filesystem::path getExecutablePath(char **argv) {
#ifdef __APPLE__
    // argv[0] is not a usable path when launched from Finder or through PATH.
    char buffer[PATH_MAX];
    uint32_t size = sizeof(buffer);
    if (_NSGetExecutablePath(buffer, &size) == 0) {
        std::error_code ec;
        const auto path = std::filesystem::canonical(buffer, ec);
        if (!ec) {
            return path;
        }
    }
#endif
    return std::filesystem::absolute(argv[0]);
}

bool validateStartDir(char **argv) {
    if (isValidStartDir()) {
        return true;
    }

    const std::filesystem::path exeDir = getExecutablePath(argv).parent_path();
    std::filesystem::current_path(exeDir);

#ifdef __APPLE__
    // Inside Bess.app the executable is in Contents/MacOS and the assets are
    // in Contents/Resources.
    if (!isValidStartDir() &&
        std::filesystem::exists(exeDir / ".." / "Resources" / "assets")) {
        std::filesystem::current_path(exeDir / ".." / "Resources");
    }
#endif

    return isValidStartDir();
}
