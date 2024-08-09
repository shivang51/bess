#include "application.h"
#include <filesystem>
#include <iostream>

static bool isValidStartDir() {
    auto cwd = std::filesystem::current_path();
    return std::filesystem::exists(std::filesystem::path(cwd.string() + "/assets"));
}

int main(int argc, char** argv) {
    std::vector<std::string> args(argv, argv + argc);
    
    if (!isValidStartDir()) {
        std::filesystem::current_path(std::filesystem::path(args[0]).parent_path());
        if (!isValidStartDir()) {
            std::cerr << "[-] Wrong current working directory!. Exiting now." << std::endl;
            return -1;
        }
    }

    Bess::Application app = Bess::Application();
    app.run();
    return 0;
}
