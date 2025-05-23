#define FMT_UNICODE 0
#include "application.h"
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <iostream>

#ifdef __linux__
    #include <stacktrace>

void signalHandler(int sig) {
    std::cerr << "Error: signal " << sig << std::endl;
    std::stacktrace st = std::stacktrace::current(10);
    std::cout << st << std::endl;
    exit(1);
}

#endif // _LINUX

static bool isValidStartDir() {
    return std::filesystem::exists("assets");
}

int main(int argc, char **argv) {
    std::vector<std::string> args(argv, argv + argc);

    if (!isValidStartDir()) {
        std::filesystem::path exePath = std::filesystem::absolute(argv[0]);
        std::filesystem::path exeDir = exePath.parent_path();
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
    try {
        app.init("");
        app.run();
    } catch (const std::exception &e) {
        std::cerr << e.what() << std::endl;
        app.quit();
        return -1;
    }
    return 0;
}
