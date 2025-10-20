#define FMT_UNICODE 0
#include "application.h"
#include "component_definition.h"
#include "plugin_manager.h"
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <iostream>

#ifdef __linux__
    #include <stacktrace>

void signalHandler(int sig) {
    std::cerr << "Error: signal " << sig << std::endl;
    const std::stacktrace st = std::stacktrace::current();
    std::cout << st << std::endl;
    exit(1);
}

#endif // _LINUX

static bool isValidStartDir() {
    return std::filesystem::exists("assets");
}

int main(int argc, char **argv) {
    if (!isValidStartDir()) {
        const std::filesystem::path exePath = std::filesystem::absolute(argv[0]);
        const std::filesystem::path exeDir = exePath.parent_path();
        std::filesystem::current_path(exeDir);

        if (!isValidStartDir()) {
            std::cerr << "[-] Wrong working directory. Expected 'assets/' folder. Exiting." << std::endl;
            return -1;
        }
    }

#ifdef __linux__
    #ifndef NDEBUG
    struct sigaction sa;
    sa.sa_handler = signalHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGSEGV, &sa, nullptr);
    #endif
#endif // _LINUX

    Bess::Application app;
    Bess::AppStartupFlags flags = Bess::AppStartupFlag::none;

    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--disable-plugins") {
            flags |= Bess::AppStartupFlag::disablePlugins;
        }
    }

    try {
        const char *projectFile = (argc == 2) ? argv[1] : "";
        app.init(projectFile, flags);
        app.run();
    } catch (const std::exception &e) {
        std::cerr << e.what() << std::endl;
        app.quit();
        return -1;
    }
    return 0;
}
