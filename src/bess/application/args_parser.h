#pragma once

#include <iostream>
#include <string>

struct AppArgs {
    std::string projectFile;
    bool disablePlugins = false;
};

/* Parse command-line arguments and fills outArgs param.
 Returns true on success, false on failure.
*/
static bool parseArgs(int argc, char **argv, AppArgs &outArgs) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg.size() > 2 && arg.starts_with("--")) {
            if (arg == "--no-plugins") {
                outArgs.disablePlugins = true;
            }
        } else {
            outArgs.projectFile = arg;
        }
    }

    return true;
}

static void printUsage(const char *exeName) {
    std::cout << "Usage: " << exeName << " [options] [project_file]" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  --disable-plugins    Disable loading of plugins" << std::endl;
}
