#include "application/project_file.h"
#include "common/bess_uuid.h"
#include "common/logger.h"
#include "pages/main_page/scene_components/scene_comp_types.h"
#include "pages/main_page/scene_components/sim_scene_component.h"
#include "plugin_manager.h"

#include "pages/main_page/main_page.h"
#include "scene.h"
#include "simulation_engine.h"
#include "ui/ui_main/dialogs.h"

#include "json/value.h"
#include <filesystem>
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

        const auto &sceneDriver = Pages::MainPage::getInstance()->getState().getSceneDriver();
        JsonConvert::toJsonValue(sceneDriver.getRootSceneId(), data["scene_data"]["root_scene_id"]);

        data["scene_data"]["scenes"] = Json::arrayValue;
        for (const auto &scene : sceneDriver.getScenes()) {
            m_sceneSerializer.serialize(data["scene_data"]["scenes"].append(Json::objectValue),
                                        scene);
        }

        m_simEngineSerializer.serialize(data["sim_engine_data"]);

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
            BESS_ERROR("Failed to open file for reading: {}", m_path);
            return;
        }
        Json::Value data;
        std::string errs;

        Json::CharReaderBuilder builder;
        bool parsingSuccessful = Json::parseFromStream(builder, inFile, &data, &errs);

        if (!parsingSuccessful) {
            BESS_ERROR("Failed to parse JSON from {}\n{}", m_path, errs);
            return;
        }

        auto &simEngine = SimEngine::SimulationEngine::instance();
        simEngine.setSimulationState(SimEngine::SimulationState::paused);
        m_name = data.get("name", "Unnamed Project").asString();
        if (data.isMember("sim_engine_data")) {
            m_simEngineSerializer.deserialize(data["sim_engine_data"]);
            BESS_DEBUG("Derserialzed Sim Engine Data");
        }

        // make sure to decode scene after sim engine,
        // as scene components may depend on sim engine components
        if (data.isMember("scene_data")) {
            auto &sceneDriver = Pages::MainPage::getInstance()->getState().getSceneDriver();

            sceneDriver.removeScenes();

            BESS_DEBUG("[Decode] Cleared Scenes. count = {}", sceneDriver.getSceneCount());

            auto &simEngine = SimEngine::SimulationEngine::instance();

            const auto &pluginManager = Plugins::PluginManager::getInstance();

            // decoding scenes
            for (auto &sceneJson : data["scene_data"]["scenes"]) {
                auto scene = std::make_shared<Canvas::Scene>();
                m_sceneSerializer.deserialize(sceneJson, scene);
                sceneDriver.addScene(scene);

                auto &sceneState = scene->getState();

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
                BESS_DEBUG("[Decode] Added new scene {}", (uint64_t)scene->getSceneId());
            }

            // setting root scene id
            JsonConvert::fromJsonValue(data["scene_data"]["root_scene_id"],
                                       sceneDriver.getRootSceneId());

            sceneDriver.makeRootSceneActive();
            simEngine.setSimulationState(SimEngine::SimulationState::running);
        }
    }

    void ProjectFile::browsePath() {
        const auto pathStr = UI::Dialogs::showSaveFileDialog("Save To", "");

        if (pathStr.empty()) {
            BESS_WARN("No path selected");
            return;
        }

        m_path = pathStr;

        const auto path = std::filesystem::path(pathStr);
        m_name = path.filename();

        if (!m_path.ends_with(".bproj")) {
            m_path += ".bproj";
        } else {
            m_name = m_name.substr(0, m_name.size() - 6);
        }

        BESS_INFO("Project path {} selected with name {}", m_path, m_name);
    }

    void ProjectFile::patchFile() const {
    }
} // namespace Bess
