#include "application.h"
#include <csignal>
#include <cstdlib>
#include <execinfo.h>
#include <filesystem>
#include <iostream>
#include <unistd.h>

static bool isValidStartDir() {
    auto cwd = std::filesystem::current_path();
    return std::filesystem::exists(std::filesystem::path(cwd.string() + "/assets"));
}

static std::string binary;

void print_stacktrace() {
    void *array[10];
    size_t size = backtrace(array, 10);
    char **symbols = backtrace_symbols(array, size);
    for (size_t i = 0; i < size; ++i) {
        std::cerr << symbols[i] << std::endl;
        // Extract the address and resolve it using addr2line
        char command[256];
        snprintf(command, sizeof(command), "addr2line -e %s %p", binary.c_str(), array[i]);
        system(command); // Run addr2line for each address
    }
    free(symbols);
}

void signal_handler(int sig) {
    std::cerr << "Error: signal " << sig << std::endl;
    print_stacktrace();
    exit(1);
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

#ifndef NDEBUG
    binary = args[0];
    std::cout << "[+] Debug mode for " << binary << std::endl;
    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGSEGV, &sa, nullptr);
#endif

    Bess::Application app = Bess::Application();
    try {
        app.run();
    } catch (const std::exception &e) {
        std::cerr << e.what() << std::endl;
        app.quit();
    }
    return 0;
}
