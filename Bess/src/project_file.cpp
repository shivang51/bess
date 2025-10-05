#include "project_file.h"
#include "bess_uuid.h"
#include "common/log.h"
#include "json/json.h"

#include "scene/components/components.h"
#include "scene/scene.h"
#include "ui/ui_main/dialogs.h"

#include <cstdint>
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

        m_sceneSerializer.serialize(data["scene_data"]);
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

        if (data.isMember("scene_data")) {
            m_sceneSerializer.deserialize(data["scene_data"]);
        }
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
        using namespace Bess::Canvas;
        auto scene = Canvas::Scene::instance();
        auto &reg = scene->getEnttRegistry();

        for (auto &ent : reg.view<entt::entity>()) {
            auto *comp = reg.try_get<Components::TagComponent>(ent);
            if (!comp)
                continue;
            BESS_WARN("Running patch for {}", comp->name);
            if (auto *simComp = reg.try_get<Components::SimulationComponent>(ent)) {
                try {
                    BESS_WARN("Running patch for SimulationComponent of {}", (uint64_t)simComp->simEngineEntity);
                    auto &simEngine = Bess::SimEngine::SimulationEngine::instance();
                    auto &idComp = reg.get<Components::IdComponent>(ent);

                    if (comp->type.simCompType == Bess::SimEngine::ComponentType::EMPTY) {
                        BESS_WARN("Patching empty component type...");
                        comp->type.simCompType = simEngine.getComponentType(simComp->simEngineEntity);
                    }

                    simComp->type = comp->type.simCompType;

                    if ((comp->type.simCompType == SimEngine::ComponentType::FLIP_FLOP_JK ||
                         comp->type.simCompType == SimEngine::ComponentType::FLIP_FLOP_SR) &&
                        simComp->inputSlots.size() != 4) {
                        BESS_WARN("Patching flip flop input count...");

                        simEngine.updateInputCount(simComp->simEngineEntity, 4);
                        simComp->inputSlots.emplace_back(scene->createSlotEntity(Components::SlotType::digitalInput, idComp.uuid, 3));
                    } else if ((comp->type.simCompType == SimEngine::ComponentType::FLIP_FLOP_D ||
                                comp->type.simCompType == SimEngine::ComponentType::FLIP_FLOP_T) &&
                               simComp->inputSlots.size() != 3) {
                        BESS_WARN("Patching flip flop input count...");
                        simEngine.updateInputCount(simComp->simEngineEntity, 3);
                        simComp->inputSlots.emplace_back(scene->createSlotEntity(Components::SlotType::digitalInput, idComp.uuid, 2));
                    }

                    auto connView = reg.view<Components::IdComponent, Components::ConnectionComponent>();

                    for (const auto slotUuid : simComp->inputSlots) {
                        auto &slotComp = reg.get<Components::SlotComponent>(scene->getEntityWithUuid(slotUuid));
                        if (!slotComp.connections.empty())
                            continue;
                        std::set<UUID> connections = {};
                        for (const auto connEnt : connView) {
                            const auto &conn = connView.get<Components::ConnectionComponent>(connEnt);
                            if (slotUuid == conn.inputSlot || slotUuid == conn.outputSlot) {
                                const auto &id = connView.get<Components::IdComponent>(connEnt);
                                connections.insert(id.uuid);
                            }
                        }
                        if (connections.empty())
                            continue;
                        BESS_WARN("Patching {} connections for slot {}", connections.size(), (uint64_t)slotUuid);
                        slotComp.connections.insert(slotComp.connections.begin(), connections.begin(), connections.end());
                    }

                    for (const auto slotUuid : simComp->outputSlots) {
                        auto &slotComp = reg.get<Components::SlotComponent>(scene->getEntityWithUuid(slotUuid));
                        if (!slotComp.connections.empty())
                            continue;
                        std::set<UUID> connections = {};
                        for (const auto connEnt : connView) {
                            const auto &conn = connView.get<Components::ConnectionComponent>(connEnt);
                            if (slotUuid == conn.inputSlot || slotUuid == conn.outputSlot) {
                                const auto &id = connView.get<Components::IdComponent>(connEnt);
                                connections.insert(id.uuid);
                            }
                        }
                        if (connections.empty())
                            continue;
                        BESS_WARN("Patching {} connections for slot {}", connections.size(), (uint64_t)slotUuid);
                        slotComp.connections.insert(slotComp.connections.begin(), connections.begin(), connections.end());
                    }

                    BESS_WARN("(Done)");
                } catch (std::exception e) {
                    BESS_ERROR("(Failed)");
                }
            }
            if (auto *spriteComp = reg.try_get<Components::SpriteComponent>(ent)) {
                BESS_WARN("Running patch for SpriteComponent");
                auto expectedColor = ViewportTheme::getCompHeaderColor(comp->type.simCompType);
                if (spriteComp->headerColor != expectedColor) {
                    spriteComp->headerColor = expectedColor;
                    BESS_WARN("Fixed Header Color");
                }
            }
        }
    }
} // namespace Bess
