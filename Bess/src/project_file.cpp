#include "project_file.h"
#include "json.hpp"

#include "scene/scene_serializer.h"
#include "simulation_engine_serializer.h"
#include "ui/ui_main/dialogs.h"

#include <fstream>
#include <iostream>

namespace Bess {
    ProjectFile::ProjectFile() {
        m_name = "Unnamed";
        m_saved = false;
    }

    ProjectFile::ProjectFile(const std::string &path) {
        std::cout << "[Bess] Opening project " << path << std::endl;
        m_path = path;
        decode();
        m_saved = true;
        std::cout << "[Bess] Project Loaded Successfully" << path << std::endl;
    }

    ProjectFile::~ProjectFile() {
    }

    void ProjectFile::save() {
        if (m_path == "") {
            browsePath();
            if (m_path == "")
                return;
        }

        std::cout << "[Bess] Saving project " << m_path << std::endl;
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
        m_path = path;
        m_name = path.substr(path.find_last_of("/\\") + 1);
        std::cout << "[Bess] Project path: " << m_path << " with name " << m_name << std::endl;
    }
} // namespace Bess
