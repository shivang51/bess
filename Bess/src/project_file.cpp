#include "project_file.h"
#include "common/log.h"
#include "json.hpp"

#include "scene/components/components.h"
#include "scene/scene.h"
#include "scene/scene_serializer.h"
#include "simulation_engine_serializer.h"
#include "ui/ui_main/dialogs.h"

#include <fstream>

namespace Bess {
    ProjectFile::ProjectFile() {
        m_name = "Unnamed";
        m_saved = false;
    }

    ProjectFile::ProjectFile(const std::string &path) {
        BESS_INFO("Opening project from {}", path);
        m_path = path;
        decode();
        patchFile();
        m_saved = true;
        BESS_INFO("Project Loaded Successfully");
    }

    ProjectFile::~ProjectFile() {
    }

    void ProjectFile::save() {
        if (m_path == "") {
            browsePath();
            if (m_path == "")
                return;
        }

        BESS_INFO("Saving project {}", m_path);
        auto data = encode();
        std::ofstream o(m_path);
        o << std::setw(4) << data << std::endl;
        m_saved = true;
    }

    const std::string &ProjectFile::getName() const {
        return m_name;
    }

    std::string &ProjectFile::getNameRef() {
        return m_name;
    }

    void ProjectFile::setName(const std::string &name) {
        m_name = name;
    }

    const std::string &ProjectFile::getPath() const {
        return m_path;
    }

    void ProjectFile::setPath(const std::string &path) {
        m_path = path;
    }

    bool ProjectFile::isSaved() {
        return m_saved;
    }

    nlohmann::json ProjectFile::encode() {
        nlohmann::json data;
        data["name"] = m_name;
        data["version"] = "<dev>";
        data["scene_data"] = SceneSerializer().serialize();
        data["sim_engine_data"] = SimEngine::SimEngineSerializer().serialize();
        return data;
    }

    void ProjectFile::decode() {
        std::ifstream inFile(m_path);
        if (!inFile.is_open()) {
            std::cerr << "Failed to open file for reading: " << m_path << std::endl;
            return;
        }
        nlohmann::json data;
        inFile >> data;

        m_name = data["name"];

        SimEngine::SimEngineSerializer().deserialize(data["sim_engine_data"]);
        SceneSerializer().deserialize(data["scene_data"]);
    }

    void ProjectFile::browsePath() {
        auto path = UI::Dialogs::showSaveFileDialog("Save To", "");
        if (path.size() == 0) {
            BESS_WARN("No path selected");
            return;
        }
        m_path = path;
        m_name = path.substr(path.find_last_of("/\\") + 1);
        BESS_INFO("Project path {} selected with name {}", m_path, m_name);
    }

    void ProjectFile::patchFile() {
        BESS_INFO("Running Patch...");
        using namespace Bess::Canvas;
        auto &scene = Canvas::Scene::instance();
        auto &reg = scene.getEnttRegistry();

        for (auto &ent : reg.view<entt::entity>()) {
            if (auto *comp = reg.try_get<Components::TagComponent>(ent)) {
                // if component type is not there in tag, then query it and add it
                if (comp->isSimComponent && comp->type.simCompType == Bess::SimEngine::ComponentType::EMPTY) {
                    auto *simComp = reg.try_get<Components::SimulationComponent>(ent);
                    if (simComp == nullptr)
                        continue;
                    try {
                        BESS_INFO("Patching empty component type...");
                        auto &simEngine = Bess::SimEngine::SimulationEngine::instance();
                        comp->type.simCompType = simEngine.getComponentType(simComp->simEngineEntity);
                        BESS_INFO("(Done)");
                    } catch (std::exception e) {
                        BESS_ERROR("(Failed)");
                    }
                }
            }
        }
    }
} // namespace Bess
