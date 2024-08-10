#include "application.h"
#include <filesystem>
#include <iostream>

#include "project_file.h"
#include "components_manager/component_bank.h"

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

    //Bess::ProjectFile file("dummy.bproj");
    //file.getName();
    //Bess::Simulator::ComponentBank::loadFromJson("assets/gates_collection.json");

    //Bess::Simulator::ComponentsManager::generateComponent(Bess::Simulator::ComponentType::inputProbe);
    //Bess::Simulator::ComponentsManager::generateComponent(Bess::Simulator::ComponentType::outputProbe);
    //Bess::Simulator::ComponentsManager::generateComponent(Bess::Simulator::ComponentType::jcomponent, Bess::Simulator::ComponentBank::getJCompData("Digital Gates", "NAND Gate"));

    //Bess::ProjectFile file;
    //file.setName("Test Project");
    //file.setPath("./test.bproj");
    //file.save();

    Bess::Application app = Bess::Application();
    app.loadProject("test.bproj");
    app.run();
    return 0;
}
