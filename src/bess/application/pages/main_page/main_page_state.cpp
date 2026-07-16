#include "pages/main_page/main_page_state.h"

#include "bess_core/commands/update_value_command.h"
#include "bess_core/g_app_context.h"
#include "bess_core/project_context.h"
#include "bess_core/scene/scene.h"
#include "bess_core/scene_driver.h"
#include "bverilog/sim_engine_importer.h"
#include "common/bess_uuid.h"
#include "common/events.h"
#include "common/logger.h"
#include "event_dispatcher.h"
#include "pages/main_page/main_page.h"
#include "pages/main_page/main_page_command_hooks.h"
#include "pages/main_page/scene_components/image_scene_component.h"
#include "pages/main_page/scene_components/scene_comp_types.h"
#include "pages/main_page/scene_components/sim_scene_component.h"
#include "pages/main_page/scene_components/slot_probe_scene_component.h"
#include "pages/main_page/scene_components/slot_scene_component.h"
#include "pages/main_page/verilog_scene_import.h"
#include "simulation_engine.h"
#include "stb_image.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <memory>
#include <optional>

namespace Bess::Pages {
    namespace {
        std::string fallbackSlotName(size_t index, bool isInput) {
            const char base = isInput ? 'A' : 'a';
            return {static_cast<char>(base + (index % 26))};
        }

        std::shared_ptr<Canvas::Scene>
        getTrackedScene(const std::shared_ptr<SceneDriver> &sceneDriver,
                        const UUID &sceneId) {
            if (sceneId == UUID::null) {
                return nullptr;
            }

            return sceneDriver->getSceneWithId(sceneId);
        }

        void syncSceneComponentSlots(
            Canvas::SceneState &sceneState,
            const std::shared_ptr<Canvas::SimulationSceneComponent> &comp,
            const SimEngine::PortDescriptor &portDescriptor,
            bool isInput) {
            if (!comp) {
                return;
            }

            const auto &slotIds =
                isInput ? comp->getInputSlots() : comp->getOutputSlots();
            const auto direction = isInput ? SimEngine::PortDirection::input
                                           : SimEngine::PortDirection::output;
            const auto signalKind = portDescriptor.signalKind;

            std::vector<UUID> realSlots;
            realSlots.reserve(slotIds.size());
            UUID resizeSlotId = UUID::null;

            for (const auto &slotId : slotIds) {
                const auto slot =
                    sceneState.getComponentByUuid<Canvas::SlotSceneComponent>(
                        slotId);
                if (!slot) {
                    continue;
                }

                if (slot->isResizeSlot()) {
                    resizeSlotId = slotId;
                    continue;
                }

                realSlots.push_back(slotId);
            }

            while (realSlots.size() > portDescriptor.count) {
                const auto slotId = realSlots.back();
                realSlots.pop_back();
                comp->removeChildComponent(slotId);
                sceneState.removeComponent(slotId, UUID::master);
            }

            while (realSlots.size() < portDescriptor.count) {
                auto newSlot = std::make_shared<Canvas::SlotSceneComponent>();
                newSlot->setPortDirection(direction);
                newSlot->setSignalKind(signalKind);
                newSlot->setResizeTrigger(false);
                sceneState.addComponent<Canvas::SlotSceneComponent>(newSlot);
                sceneState.attachChild(
                    comp->getUuid(), newSlot->getUuid(), false);
                realSlots.push_back(newSlot->getUuid());
            }

            if (portDescriptor.isResizeable) {
                if (resizeSlotId == UUID::null) {
                    auto resizeSlot =
                        std::make_shared<Canvas::SlotSceneComponent>();
                    resizeSlot->setPortDirection(direction);
                    resizeSlot->setSignalKind(signalKind);
                    resizeSlot->setResizeTrigger(true);
                    resizeSlot->setIndex(-1);
                    sceneState.addComponent<Canvas::SlotSceneComponent>(
                        resizeSlot);
                    sceneState.attachChild(
                        comp->getUuid(), resizeSlot->getUuid(), false);
                    resizeSlotId = resizeSlot->getUuid();
                }
            } else if (resizeSlotId != UUID::null) {
                comp->removeChildComponent(resizeSlotId);
                sceneState.removeComponent(resizeSlotId, UUID::master);
                resizeSlotId = UUID::null;
            }

            std::vector<UUID> nextSlotIds;
            nextSlotIds.reserve(realSlots.size() +
                                (resizeSlotId != UUID::null ? 1 : 0));
            nextSlotIds.insert(
                nextSlotIds.end(), realSlots.begin(), realSlots.end());
            if (resizeSlotId != UUID::null) {
                nextSlotIds.push_back(resizeSlotId);
            }

            if (isInput) {
                comp->setInputSlots(nextSlotIds);
            } else {
                comp->setOutputSlots(nextSlotIds);
            }

            for (size_t i = 0; i < realSlots.size(); ++i) {
                const auto slot =
                    sceneState.getComponentByUuid<Canvas::SlotSceneComponent>(
                        realSlots[i]);
                if (!slot) {
                    continue;
                }

                slot->setPortDirection(direction);
                slot->setSignalKind(signalKind);
                slot->setResizeTrigger(false);
                slot->setIndex(static_cast<int>(i));
                if (i < portDescriptor.names.size()) {
                    slot->setName(portDescriptor.names[i]);
                } else {
                    slot->setName(fallbackSlotName(i, isInput));
                }
            }

            if (resizeSlotId != UUID::null) {
                const auto resizeSlot =
                    sceneState.getComponentByUuid<Canvas::SlotSceneComponent>(
                        resizeSlotId);
                if (resizeSlot) {
                    resizeSlot->setPortDirection(direction);
                    resizeSlot->setSignalKind(signalKind);
                    resizeSlot->setResizeTrigger(true);
                    resizeSlot->setIndex(-1);
                    resizeSlot->setName("");
                }
            }

            comp->setScaleDirty();
            comp->setSchematicScaleDirty();
        }

        std::vector<std::filesystem::path>
        toFilesystemPaths(const std::vector<std::string> &paths) {
            std::vector<std::filesystem::path> result;
            result.reserve(paths.size());
            for (const auto &path : paths) {
                result.emplace_back(path);
            }
            return result;
        }

        std::string primaryImportPath(const std::vector<std::string> &paths) {
            if (paths.empty()) {
                return {};
            }
            return paths.front();
        }

        struct StbiImageDeleter {
            void operator()(stbi_uc *pixels) const {
                stbi_image_free(pixels);
            }
        };

        struct DecodedImage {
            std::vector<uint8_t> rgba8;
            uint32_t width = 0;
            uint32_t height = 0;
        };

        struct FileDropContext {
            std::shared_ptr<Canvas::Scene> scene;
            Canvas::SceneState *sceneState = nullptr;
            glm::vec2 basePos{0.f};
            std::vector<UUID> addedComponentIds;

            void addComponent(
                const std::shared_ptr<Canvas::SceneComponent> &component) {
                if (!scene || component == nullptr) {
                    return;
                }

                const auto offset =
                    static_cast<float>(addedComponentIds.size()) * 24.f;
                component->setPosition(
                    {basePos.x + offset, basePos.y + offset, 0.f});
                scene->addComponent(component);
                addedComponentIds.push_back(component->getUuid());
            }
        };

        std::string normalizedExtension(const std::string &path) {
            auto ext = std::filesystem::path(path).extension().string();
            std::ranges::transform(ext, ext.begin(), [](unsigned char ch) {
                return static_cast<char>(std::tolower(ch));
            });
            return ext;
        }

        bool isSupportedImageFile(const std::string &path) {
            const auto ext = normalizedExtension(path);
            return ext == ".png" || ext == ".jpg" || ext == ".jpeg" ||
                   ext == ".bmp" || ext == ".tga";
        }

        std::optional<size_t> rgbaByteCount(uint32_t width, uint32_t height) {
            if (width == 0u || height == 0u) {
                return std::nullopt;
            }

            const auto w = static_cast<size_t>(width);
            const auto h = static_cast<size_t>(height);
            if (w > std::numeric_limits<size_t>::max() / h) {
                return std::nullopt;
            }

            const auto pixels = w * h;
            if (pixels > std::numeric_limits<size_t>::max() /
                             static_cast<size_t>(4u)) {
                return std::nullopt;
            }

            return pixels * 4u;
        }

        std::optional<DecodedImage> decodeImageFile(const std::string &path) {
            int width = 0;
            int height = 0;
            int channels = 0;
            std::unique_ptr<stbi_uc, StbiImageDeleter> pixels(
                stbi_load(path.c_str(), &width, &height, &channels, 4));
            (void)channels;

            if (!pixels || width <= 0 || height <= 0) {
                BESS_ERROR("[MainPageState] Failed to load image {}: {}",
                           path,
                           stbi_failure_reason());
                return std::nullopt;
            }

            DecodedImage image;
            image.width = static_cast<uint32_t>(width);
            image.height = static_cast<uint32_t>(height);
            const auto byteCount = rgbaByteCount(image.width, image.height);
            if (!byteCount.has_value()) {
                BESS_ERROR("[MainPageState] Dropped image {} has invalid "
                           "dimensions {}x{}",
                           path,
                           width,
                           height);
                return std::nullopt;
            }

            image.rgba8.assign(pixels.get(), pixels.get() + *byteCount);
            return image;
        }

        glm::vec2 initialImageSceneScale(uint32_t width, uint32_t height) {
            constexpr float maxInitialSide = 360.f;
            glm::vec2 scale{static_cast<float>(width),
                            static_cast<float>(height)};
            const float largestSide = std::max(scale.x, scale.y);
            if (largestSide > maxInitialSide) {
                scale *= maxInitialSide / largestSide;
            }
            scale.x = std::max(16.f, scale.x);
            scale.y = std::max(16.f, scale.y);
            return scale;
        }

        bool handleDroppedImageFile(const std::string &path,
                                    FileDropContext &ctx) {
            if (!isSupportedImageFile(path)) {
                return false;
            }

            auto decodedImage = decodeImageFile(path);
            if (!decodedImage.has_value()) {
                return true;
            }

            auto imageComponent =
                std::make_shared<Canvas::ImageSceneComponent>();
            const auto fileName =
                std::filesystem::path(path).filename().string();
            imageComponent->setName(fileName.empty() ? "Image" : fileName);
            imageComponent->setImageWidth(decodedImage->width);
            imageComponent->setImageHeight(decodedImage->height);
            imageComponent->setData(decodedImage->rgba8);
            imageComponent->setMaintainAspectRatio(true);
            imageComponent->setScale(initialImageSceneScale(
                decodedImage->width, decodedImage->height));
            ctx.addComponent(imageComponent);
            return true;
        }

        bool handleDroppedFile(const std::string &path, FileDropContext &ctx) {
            using DropHandler = bool (*)(const std::string &, FileDropContext &);
            static constexpr std::array<DropHandler, 1> handlers{
                handleDroppedImageFile,
            };

            for (const auto handler : handlers) {
                if (handler(path, ctx)) {
                    return true;
                }
            }

            return false;
        }
    } // namespace

    struct MainPageState::VerilogImportSession {
        enum class Phase : uint8_t {
            resetProject,
            importSimulation,
            createScene,
            updateNets,
            completed,
            failed,
        };

        std::vector<std::string> paths;
        float progress = 0.f;
        std::string stageMessage = "Select a Verilog file";
        bool importing = false;
        bool finished = false;
        bool failed = false;
        Phase phase = Phase::resetProject;
        std::optional<Verilog::SimEngineImportResult> stagedResult;
    };

    MainPageState::MainPageState() = default;
    MainPageState::~MainPageState() = default;

    void MainPageState::resetProjectState(bool updateWindowName) {
        getSceneDriver()->reset();
        auto &appCtx = Bess::GAppContext::getInstance();
        auto projectCtx = appCtx.getSubSystem<Bess::ProjectContext>();
        projectCtx->getSimEngine().clear();
    }

    void MainPageState::createNewProject(bool updateWindowName) {
        resetProjectState(updateWindowName);

        auto &appCtx = Bess::GAppContext::getInstance();
        auto projectCtx = appCtx.getSubSystem<Bess::ProjectContext>();
        projectCtx->createNewProject();

        if (!updateWindowName)
            return;
        const auto win = MainPage::getInstance()->getParentWindow();
        win->setName("FIXME");
    }

    void MainPageState::loadProject(const std::string &path) {
        resetProjectState();

        auto &appCtx = Bess::GAppContext::getInstance();
        auto projectCtx = appCtx.getSubSystem<Bess::ProjectContext>();
        projectCtx->loadProject(path);
        const auto &project = projectCtx->getProjectFile();

        const auto win = MainPage::getInstance()->getParentWindow();
        win->setName(project->getName() + " - BESS");
    }

    void MainPageState::saveCurrentProject() const {
        auto &appCtx = Bess::GAppContext::getInstance();
        auto projectCtx = appCtx.getSubSystem<Bess::ProjectContext>();
        projectCtx->saveProject();
    }

    bool MainPageState::importVerilogFile(const std::string &path,
                                          std::string *errorMessage) {
        return importVerilogFiles(std::vector<std::string>{path}, errorMessage);
    }

    bool
    MainPageState::importVerilogFiles(const std::vector<std::string> &paths,
                                      std::string *errorMessage) {
        try {
            if (paths.empty()) {
                if (errorMessage) {
                    *errorMessage = "No Verilog files were selected";
                }
                return false;
            }

            resetProjectState();
            auto scene = getSceneDriver()->getActiveScene();
            if (!scene) {
                if (errorMessage) {
                    *errorMessage = "No active scene available";
                }
                return false;
            }

            auto &appCtx = Bess::GAppContext::getInstance();
            auto projectCtx = appCtx.getSubSystem<Bess::ProjectContext>();
            auto &simEngine = projectCtx->getSimEngine();
            const auto result = Verilog::importVerilogFilesIntoSimulationEngine(
                toFilesystemPaths(paths), simEngine);
            populateSceneFromVerilogImportResult(result, simEngine, *scene);
            updateNets(scene);

            return true;
        } catch (const std::exception &ex) {
            if (errorMessage) {
                *errorMessage = ex.what();
            }
            BESS_ERROR("[MainPageState] Failed to import Verilog files "
                       "(primary {}): {}",
                       primaryImportPath(paths),
                       ex.what());
            return false;
        }
    }

    HierarchicalSceneLayoutResult
    MainPageState::applyHierarchicalLayoutToActiveScene() {
        HierarchicalSceneLayoutResult result;
        const auto activeScene = getSceneDriver()->getActiveScene();
        if (!activeScene) {
            return result;
        }

        auto &appCtx = Bess::GAppContext::getInstance();
        auto projectCtx = appCtx.getSubSystem<Bess::ProjectContext>();
        return applyHierarchicalSceneLayout(*activeScene,
                                            projectCtx->getSimEngine());
    }

    void MainPageState::startVerilogImport(const std::string &path) {
        startVerilogImport(std::vector<std::string>{path});
    }

    void
    MainPageState::startVerilogImport(const std::vector<std::string> &paths) {
        m_verilogImportSession = std::make_unique<VerilogImportSession>();
        m_verilogImportSession->paths = paths;
        m_verilogImportSession->progress = 0.05f;
        m_verilogImportSession->stageMessage = "Clearing current project";
        m_verilogImportSession->importing = true;
        m_verilogImportSession->phase =
            VerilogImportSession::Phase::resetProject;
    }

    VerilogImportStatus
    MainPageState::advanceVerilogImport(std::string *errorMessage) {
        VerilogImportStatus status;
        if (!m_verilogImportSession) {
            status.stageMessage = "No active import";
            status.finished = true;
            status.failed = true;
            return status;
        }

        auto &session = *m_verilogImportSession;
        status.progress = session.progress;
        status.stageMessage = session.stageMessage;
        status.importing = session.importing;
        status.finished = session.finished;
        status.failed = session.failed;

        if (!session.importing || session.finished) {
            return status;
        }

        try {
            auto &appCtx = Bess::GAppContext::getInstance();
            auto projectCtx = appCtx.getSubSystem<Bess::ProjectContext>();
            auto &simEngine = projectCtx->getSimEngine();
            auto scene = getSceneDriver()->getActiveScene();

            switch (session.phase) {
            case VerilogImportSession::Phase::resetProject:
                resetProjectState();
                session.progress = 0.2f;
                session.stageMessage = "Converting Verilog to simulation graph";
                session.phase = VerilogImportSession::Phase::importSimulation;
                break;
            case VerilogImportSession::Phase::importSimulation:
                session.stagedResult =
                    Verilog::importVerilogFilesIntoSimulationEngine(
                        toFilesystemPaths(session.paths), simEngine);
                session.progress = 0.7f;
                session.stageMessage = "Creating scene components";
                session.phase = VerilogImportSession::Phase::createScene;
                break;
            case VerilogImportSession::Phase::createScene: {
                if (!scene) {
                    throw std::runtime_error("No active scene available");
                }
                auto res = session.stagedResult;
                if (!res.has_value()) {
                    throw std::runtime_error(
                        "No staged result available for scene population");
                }
                populateSceneFromVerilogImportResult(
                    res.value(), simEngine, *scene);
                session.progress = 0.92f;
                session.stageMessage = "Updating nets";
                session.phase = VerilogImportSession::Phase::updateNets;

            } break;
            case VerilogImportSession::Phase::updateNets:
                if (!scene) {
                    throw std::runtime_error("No active scene available");
                }
                updateNets(scene);
                session.progress = 1.f;
                session.stageMessage = "Import complete";
                session.phase = VerilogImportSession::Phase::completed;
                session.importing = false;
                session.finished = true;
                session.failed = false;
                session.stagedResult.reset();
                break;
            case VerilogImportSession::Phase::completed:
            case VerilogImportSession::Phase::failed:
                break;
            }
        } catch (const std::exception &ex) {
            session.importing = false;
            session.finished = true;
            session.failed = true;
            session.phase = VerilogImportSession::Phase::failed;
            session.progress = 1.f;
            session.stageMessage = std::format("Import failed: {}", ex.what());
            session.stagedResult.reset();
            if (errorMessage) {
                *errorMessage = ex.what();
            }
        }

        status.progress = session.progress;
        status.stageMessage = session.stageMessage;
        status.importing = session.importing;
        status.finished = session.finished;
        status.failed = session.failed;
        return status;
    }

    void MainPageState::cancelVerilogImport() {
        m_verilogImportSession.reset();
    }

    std::shared_ptr<ProjectFile> MainPageState::getCurrentProjectFile() const {
        const auto &appCtx = GAppContext::getInstance();
        auto projectCtx = appCtx.getSubSystem<Bess::ProjectContext>();
        return projectCtx->getProjectFile();
    }

    void MainPageState::init() {
        getCommandSystem().setSceneComponentHooks(createMainPageCommandHooks());

        auto &appCtx = GAppContext::getInstance();
        auto dispatcher = appCtx.getSubSystem<EventSystem::EventDispatcher>();

        dispatcher->sink<Events::WindowDropEvent>()
            .connect<&MainPageState::onWindowDropped>(this);
        dispatcher->sink<Canvas::Events::EntityMovedEvent>()
            .connect<&MainPageState::onEntityMoved>(this);
        dispatcher->sink<Canvas::Events::EntityReparentedEvent>()
            .connect<&MainPageState::onEntityReparented>(this);
        dispatcher->sink<Canvas::Events::ComponentAddedEvent>()
            .connect<&MainPageState::onEntityAdded>(this);
        dispatcher->sink<Canvas::Events::ComponentRemovedEvent>()
            .connect<&MainPageState::onEntityRemoved>(this);
        dispatcher->sink<SimEngine::Events::CompDefOutputsResizedEvent>()
            .connect<&MainPageState::onCompDefOutputsResized>(this);
        dispatcher->sink<SimEngine::Events::CompDefInputsResizedEvent>()
            .connect<&MainPageState::onCompDefInputsResized>(this);
    }

    void MainPageState::onWindowDropped(const Events::WindowDropEvent &event) {
        if (!event.payload || event.payload->paths.empty()) {
            return;
        }

        auto sceneDriver = getSceneDriver();
        if (!sceneDriver) {
            BESS_ERROR("SceneDriver subsystem is not available");
            return;
        }

        auto activeScene = sceneDriver->getActiveScene();
        if (!activeScene) {
            BESS_ERROR("No active scene available for dropped files");
            return;
        }

        auto &sceneState = activeScene->getState();
        FileDropContext dropCtx{
            .scene = activeScene,
            .sceneState = &sceneState,
            .basePos = sceneState.getMousePos(),
        };
        size_t handledCount = 0;

        for (const auto &filePath : event.payload->paths) {
            if (handleDroppedFile(filePath, dropCtx)) {
                ++handledCount;
                continue;
            }

            BESS_DEBUG("[MainPageState] Ignored dropped file with unsupported "
                       "type: {}",
                       filePath);
        }

        if (!dropCtx.addedComponentIds.empty()) {
            sceneState.clearSelectedComponents();
            for (const auto &componentId : dropCtx.addedComponentIds) {
                sceneState.addSelectedComponent(componentId);
            }
        } else if (handledCount == 0) {
            BESS_DEBUG("[MainPageState] No supported files found in drop");
        }
    }

    void
    MainPageState::onEntityMoved(const Canvas::Events::EntityMovedEvent &e) {
        auto entity =
            getSceneDriver()->getActiveScene()->getState().getComponentByUuid(
                e.entityUuid);
        if (!entity) {
            return;
        }

        glm::vec3 *posPtr = &entity->getTransform().position;
        auto cmd = std::make_unique<Cmd::UpdateValCommand<glm::vec3>>(
            posPtr, e.newPos, e.oldPos);
        m_commandSystem.push(std::move(cmd));
    }

    void MainPageState::onEntityReparented(
        const Canvas::Events::EntityReparentedEvent &e) {
        const auto scene = getTrackedScene(getSceneDriver(), e.sceneId);
        if (!scene) {
            return;
        }

        auto &sceneState = scene->getState();
        auto entity = sceneState.getComponentByUuid(e.entityUuid);
        if (!entity) {
            return;
        }

        if (entity->getType() == Canvas::SceneComponentType::slot) {
            return;
        }

        UUID *parentPtr = &entity->getParentComponent();

        bool parentIsWasGroup = false;

        if (e.newParentUuid != UUID::null) {
            const auto &parentComp =
                sceneState.getComponentByUuid(e.newParentUuid);
            if (!parentComp) {
                return;
            }
            parentIsWasGroup =
                parentComp->getType() == Canvas::SceneComponentType::group;
        }

        if (!parentIsWasGroup && e.prevParent != UUID::null) {
            const auto &prevParentComp =
                sceneState.getComponentByUuid(e.prevParent);
            if (!prevParentComp)
                return;
            if (prevParentComp->getType() ==
                Canvas::SceneComponentType::group) {
                parentIsWasGroup = true;
            }
        }

        // ignore if parent is/was a group
        // group handels this shit it self
        if (parentIsWasGroup) {
            return;
        }

        /// undo redo callback
        const auto callback =
            [this, entityUuid = e.entityUuid, sceneId = e.sceneId](
                bool isUndo, const UUID &newParent) {
                (void)isUndo;
                if (newParent == UUID::null)
                    return;

                const auto scene = getTrackedScene(getSceneDriver(), sceneId);
                if (!scene) {
                    return;
                }

                const auto &parent =
                    scene->getState().getComponentByUuid(newParent);
                if (!parent) {
                    return;
                }
                parent->addChildComponent(entityUuid);
            };

        if (entity) {
            auto cmd = std::make_unique<Cmd::UpdateValCommand<UUID>>(
                parentPtr, e.newParentUuid, e.prevParent, callback);
            m_commandSystem.push(std::move(cmd));
        }
    }

    std::shared_ptr<SceneDriver> MainPageState::getSceneDriver() const {
        const auto &appCtx = GAppContext::getInstance();
        return appCtx.getSubSystem<Bess::ProjectContext>()
            ->getSubSystem<SceneDriver>();
    }

    std::shared_ptr<SceneDriver> MainPageState::getSceneDriver() {
        const auto &appCtx = GAppContext::getInstance();
        return appCtx.getSubSystem<Bess::ProjectContext>()
            ->getSubSystem<SceneDriver>();
    }

    Cmd::CommandSystem &MainPageState::getCommandSystem() {
        const auto &appCtx = GAppContext::getInstance();
        auto cmdSystem = appCtx.getSubSystem<ProjectContext>()
                             ->getSubSystem<Cmd::CommandSystem>();
        return *(cmdSystem.get());
    }

    void MainPageState::update() {
        m_releasedKeysFrame.clear();
        m_pressedKeysFrame.clear();
    }

    void MainPageState::onCompDefOutputsResized(
        const SimEngine::Events::CompDefOutputsResizedEvent &e) {
        if (!m_simIdToSceneCompId.contains(e.componentId)) {
            BESS_WARN("Ignoring CompDefOutputsResizedEvent for unknown "
                      "componentId: {}",
                      (uint64_t)e.componentId);
            return;
        }

        const auto &compData = m_simIdToSceneCompId[e.componentId];

        const auto scene = getSceneDriver()->getSceneWithId(compData.sceneId);
        if (!scene) {
            m_simIdToSceneCompId.erase(e.componentId);
            return;
        }

        auto &sceneState = scene->getState();
        const auto &comp =
            sceneState.getComponentByUuidSP<Canvas::SimulationSceneComponent>(
                compData.sceneCompId);
        if (!comp)
            return; // most likely the component was deleted, so we can ignore
                    // this event

        auto &appCtx = Bess::GAppContext::getInstance();
        auto projectCtx = appCtx.getSubSystem<Bess::ProjectContext>();
        const auto &def =
            projectCtx->getSimEngine().getComponentDefinition(e.componentId);
        if (!def) {
            return;
        }

        syncSceneComponentSlots(
            sceneState, comp, def->getOutputPortDescriptor(), false);
    }

    void MainPageState::onCompDefInputsResized(
        const SimEngine::Events::CompDefInputsResizedEvent &e) {
        if (!m_simIdToSceneCompId.contains(e.componentId)) {
            BESS_WARN("Ignoring CompDefInputsResizedEvent for unknown "
                      "componentId: {}",
                      (uint64_t)e.componentId);
            return;
        }

        const auto &compData = m_simIdToSceneCompId[e.componentId];

        const auto scene = getSceneDriver()->getSceneWithId(compData.sceneId);
        if (!scene) {
            m_simIdToSceneCompId.erase(e.componentId);
            return;
        }

        auto &sceneState = scene->getState();
        const auto &comp =
            sceneState.getComponentByUuidSP<Canvas::SimulationSceneComponent>(
                compData.sceneCompId);
        if (!comp)
            return; // most likely the component was deleted, so we can ignore
                    // this event

        auto &appCtx = Bess::GAppContext::getInstance();
        auto projectCtx = appCtx.getSubSystem<Bess::ProjectContext>();
        const auto &def =
            projectCtx->getSimEngine().getComponentDefinition(e.componentId);
        if (!def) {
            return;
        }

        syncSceneComponentSlots(
            sceneState, comp, def->getInputPortDescriptor(), true);
    }

    void
    MainPageState::onEntityAdded(const Canvas::Events::ComponentAddedEvent &e) {
        const auto scene = getTrackedScene(getSceneDriver(), e.sceneId);
        if (!scene) {
            return;
        }

        const auto &sceneState = scene->getState();
        const auto &comp = sceneState.getComponentByUuidSP(e.uuid);
        if (!comp) {
            return;
        }

        if (e.type == Canvas::SceneComponentType::simulation ||
            e.type == Canvas::SceneComponentType::module) {
            const auto simComp =
                std::dynamic_pointer_cast<Canvas::SimulationSceneComponent>(
                    comp);
            if (!simComp) {
                return;
            }
            const auto &simEngineId = simComp->getSimEngineId();
            m_simIdToSceneCompId[simEngineId] = {e.uuid, e.sceneId};
        }
    }

    void MainPageState::onEntityRemoved(
        const Canvas::Events::ComponentRemovedEvent &e) {
        if (e.type != Canvas::SceneComponentType::simulation &&
            e.type != Canvas::SceneComponentType::module)
            return;

        auto it =
            std::ranges::find_if(m_simIdToSceneCompId, [&e](const auto &pair) {
                return pair.second.sceneCompId == e.uuid &&
                       pair.second.sceneId == e.sceneId;
            });

        if (it != m_simIdToSceneCompId.end()) {
            m_simIdToSceneCompId.erase(it);
        }
    }

    MainPageState::TNetIdToCompMap &
    MainPageState::getNetIdToCompMap(UUID sceneId) {
        if (!m_netIdToCompMap.contains(sceneId)) {
            m_netIdToCompMap[sceneId] = TNetIdToCompMap{};
        }

        return m_netIdToCompMap.at(sceneId);
    }

    void
    MainPageState::updateNets(const std::shared_ptr<Canvas::Scene> &scene) {
        auto &appCtx = Bess::GAppContext::getInstance();
        auto projectCtx = appCtx.getSubSystem<Bess::ProjectContext>();
        auto &simEngine = projectCtx->getSimEngine();
        if (!simEngine.isNetUpdated())
            return;

        auto &mainPageState = Pages::MainPage::getInstance()->getState();
        auto &netIdToNameMap = mainPageState.getNetIdToNameMap();
        auto &netIdCompMap =
            mainPageState.getNetIdToCompMap(scene->getSceneId());
        auto &sceneState = scene->getState();

        std::unordered_map<UUID,
                           std::shared_ptr<Canvas::SimulationSceneComponent>>
            simIdToComp;

        for (const auto &[compId, comp] : sceneState.getAllComponents()) {
            if (comp->getType() == Canvas::SceneComponentType::group ||
                comp->getType() == Canvas::SceneComponentType::simulation) {
                const auto simComp =
                    comp->cast<Canvas::SimulationSceneComponent>();
                simIdToComp[simComp->getSimEngineId()] = simComp;
            }
        }

        const auto &nets = simEngine.getNetsMap();
        for (const auto &[netId, net] : nets) {
            for (const auto &simId : net.getComponents()) {
                if (simIdToComp.contains(simId)) {
                    const auto &comp = simIdToComp[simId];
                    netIdCompMap[netId].emplace_back(comp->getUuid());
                    comp->setNetId(netId);
                }
            }
        }
    }
} // namespace Bess::Pages
