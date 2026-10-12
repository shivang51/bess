#include "application.h"
#include "args_parser.h"
#include "common/app_paths.h"
#include "common/bess_assert.h"
#include "crash_detail_helper.h"
#include <filesystem>
#include <iostream>

#ifdef __APPLE__
    #include <climits>
    #include <cstdint>
    #include <cstdlib>
    #include <mach-o/dyld.h>
#endif

bool isValidStartDir();
bool validateStartDir(char **argv);
void useBundledPython(char **argv);
void useUserDataDirs(char **argv);
Bess::AppStartupFlags prepareFlags(const AppArgs &args);

int main(int argc, char **argv) {

    if (!validateStartDir(argv)) {
        std::cerr << "[-] Wrong working directory. Expected 'assets/' folder. "
                     "Exiting."
                  << std::endl;
        return -1;
    }

    useBundledPython(argv);
    useUserDataDirs(argv);

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

void useBundledPython(char **argv) {
#ifdef __APPLE__
    // Bess.app embeds Python.framework (scripts/package_macos_app.sh). Point
    // the interpreter at its stdlib instead of the build machine's prefix, and
    // keep it from writing __pycache__ into the signed bundle.
    const auto pythonHome = getExecutablePath(argv).parent_path() / ".." /
                            "Frameworks" / "Python.framework" / "Versions" /
                            "Current";
    if (std::filesystem::exists(pythonHome / "lib")) {
        setenv("PYTHONHOME", pythonHome.lexically_normal().c_str(), 1);
        setenv("PYTHONDONTWRITEBYTECODE", "1", 1);
    }
#endif
}

void useUserDataDirs(char **argv) {
#ifdef __APPLE__
    // Bess.app is signed and may run from a read-only disk image, so it must
    // not write bess.log or bess.ini into Contents/Resources. Use the standard
    // per-user locations instead; fall back to the working directory if they
    // cannot be created.
    const auto exeDir = getExecutablePath(argv).parent_path();
    const char *home = std::getenv("HOME");
    if (home == nullptr ||
        !std::filesystem::exists(exeDir / ".." / "Resources" / "assets")) {
        return;
    }

    const auto library = std::filesystem::path(home) / "Library";
    const auto logDir = library / "Logs" / "Bess";
    const auto configDir = library / "Application Support" / "Bess";

    std::error_code ec;
    std::filesystem::create_directories(logDir, ec);
    if (!ec) {
        Bess::AppPaths::setLogDirectory(logDir);
    }

    ec.clear();
    std::filesystem::create_directories(configDir, ec);
    if (!ec) {
        Bess::AppPaths::setConfigDirectory(configDir);
    }
#endif
}
