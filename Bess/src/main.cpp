#include "application.h"
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <iostream>

#ifdef __linux__
    #include <stacktrace>

static std::string binary;

void print_stacktrace() {
    std::stacktrace st = std::stacktrace::current(10);
    std::cout << st << std::endl;
}

void signal_handler(int sig) {
    std::cerr << "Error: signal " << sig << std::endl;
    print_stacktrace();
    exit(1);
}

#endif // _LINUX

static bool isValidStartDir() {
    auto cwd = std::filesystem::current_path();
    return std::filesystem::exists(std::filesystem::path(cwd.string() + "/assets"));
}

int main(int argc, char **argv) {
    std::vector<std::string> args(argv, argv + argc);

    if (!isValidStartDir()) {
        std::filesystem::current_path(std::filesystem::path(args[0]).parent_path());
        if (!isValidStartDir()) {
            std::cerr << "[-] Wrong current working directory!. Exiting now." << std::endl;
            return -1;
        }
    }

#ifdef __linux__
    #ifndef NDEBUG
    binary = args[0];
    std::cout << "[Bess] Starting in debug mode for " << binary << std::endl;
    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGSEGV, &sa, nullptr);
    #endif
#endif // _LINUX

    Bess::Application app = Bess::Application();
    try {
        app.run();
    } catch (const std::exception &e) {
        std::cerr << e.what() << std::endl;
        app.quit();
    }

    std::cout << "[Bess] Application Closed" << std::endl;
    return 0;
}
