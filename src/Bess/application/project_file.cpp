#include "application/project_file.h"
#include "common/log.h"
#include "pages/main_page/scene_components/scene_comp_types.h"
#include "pages/main_page/scene_components/sim_scene_component.h"
#include "plugin_manager.h"

#include "pages/main_page/main_page.h"
#include "simulation_engine.h"
#include "ui/ui_main/dialogs.h"
#include "ui/ui_main/project_explorer.h"

#include <fstream>

namespace Bess {
    ProjectFile::ProjectFile() : m_name("Unnamed") {
    }

    ProjectFile::ProjectFile(const std::string &path) : m_path(path), m_saved(true) {
        BESS_INFO("Opening project from {}", path);

        decode();
        patchFile();

        BESS_INFO("Project Loaded Successfully");
    }

    void ProjectFile::save() {
        if (m_path == "") {
            browsePath();
            if (m_path == "")
                return;
        }

        BESS_INFO("Saving project {}", m_path);
        encodeAndSave();
        BESS_INFO("Saving Successfull");

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

    bool ProjectFile::isSaved() const {
        return m_saved;
    }

    void ProjectFile::encodeAndSave() {
        Json::Value data;

        data["name"] = m_name;
        data["version"] = "<dev>";

        data["scene_data"] = Json::objectValue;
        data["sim_engine_data"] = Json::objectValue;

        auto scene = Pages::MainPage::getTypedInstance()->getState().getSceneDriver().getActiveScene();

        m_sceneSerializer.serialize(data["scene_data"], scene);
        m_simEngineSerializer.serialize(data["sim_engine_data"]);

        data["project_explorere_state"] = UI::ProjectExplorer::state.toJson();

        if (std::ofstream outFile(m_path, std::ios::out); outFile.is_open()) {
            Json::StreamWriterBuilder builder;
            builder["commentStyle"] = "None";
            builder["indentation"] = "    ";
            std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
            writer->write(data, &outFile);
        } else {
            BESS_ERROR("Failed to open file for writing: {}", m_path);
        }

        data.clear();
    }

    void ProjectFile::decode() {
        std::ifstream inFile(m_path);
        if (!inFile.is_open()) {
            std::cerr << "Failed to open file for reading: " << m_path << std::endl;
            return;
        }
        Json::Value data;
        std::string errs;

        Json::CharReaderBuilder builder;
        bool parsingSuccessful = Json::parseFromStream(builder, inFile, &data, &errs);

        if (!parsingSuccessful) {
            std::cerr << "Failed to parse JSON from " << m_path << ":" << std::endl
                      << errs << std::endl;
            return;
        }

        m_name = data.get("name", "Unnamed Project").asString();
        if (data.isMember("sim_engine_data")) {
            m_simEngineSerializer.deserialize(data["sim_engine_data"]);
        }

        // make sure to decode scene after sim engine,
        // as scene components may depend on sim engine components
        if (data.isMember("scene_data")) {
            auto scene = Pages::MainPage::getTypedInstance()->getState().getSceneDriver().getActiveScene();
            m_sceneSerializer.deserialize(data["scene_data"], scene);
        }

        auto &simEngine = SimEngine::SimulationEngine::instance();

        auto scene = Pages::MainPage::getTypedInstance()->getState().getSceneDriver().getActiveScene();
        auto &sceneState = scene->getState();

        const auto &pluginManager = Plugins::PluginManager::getInstance();

        float maxZ = 0;

        for (const auto &[uuid, comp] : sceneState.getAllComponents()) {
            maxZ = std::max(comp->getTransform().position.z, maxZ);
            if (comp->getType() == Canvas::SceneComponentType::simulation) {
                auto simComp = sceneState.getComponentByUuid<Canvas::SimulationSceneComponent>(uuid);
                const auto &compDef = simEngine.getComponentDefinition(simComp->getSimEngineId());
                if (scene->hasPluginDrawHookForComponentHash(compDef->getBaseHash())) {
                    simComp->setDrawHook(scene->getPluginDrawHookForComponentHash(compDef->getBaseHash()));
                }
            }
        }

        scene->setZCoord(maxZ);

        UI::ProjectExplorer::state.fromJson(data["project_explorere_state"]);
    }

    void ProjectFile::browsePath() {
        const auto path = UI::Dialogs::showSaveFileDialog("Save To", "");
        if (path.size() == 0) {
            BESS_WARN("No path selected");
            return;
        }
        m_path = path;
        m_name = path.substr(path.find_last_of("/\\") + 1);
        BESS_INFO("Project path {} selected with name {}", m_path, m_name);
    }

    void ProjectFile::patchFile() const {
    }
} // namespace Bess
