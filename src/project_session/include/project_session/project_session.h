#pragma once

#include "project_session/edit_hooks.h"
#include "project_session/project_document.h"
#include "project_session/project_transaction.h"

#include "common/sub_sys_container.h"
#include "common/sub_system.h"

#include <concepts>
#include <cstddef>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>

namespace Bess {
    class SceneDriver;

    namespace Canvas {
        class SceneComponent;
    } // namespace Canvas

    namespace SimEngine {
        class SimulationEngine;
        namespace Drivers {
            class CompDef;
        }
    } // namespace SimEngine

    struct SessOpts {
        std::size_t maxHist = 256;
        std::size_t maxHistBytes = std::size_t{64} * 1024U * 1024U;
        DocOpts doc;
    };

    struct SessView {
        StateId state = 0;
        bool undo = false;
        bool redo = false;
        bool dirty = true;
        bool busy = false;
        bool faulted = false;
        std::size_t undoCount = 0;
        std::size_t redoCount = 0;
        std::string undoName;
        std::string redoName;
    };

    class ProjectSession : public ISubSysContainer, public ISubSystem {
      public:
        explicit ProjectSession(SessOpts opts = {});
        ~ProjectSession() override;

        ProjectSession(const ProjectSession &) = delete;
        ProjectSession &operator=(const ProjectSession &) = delete;
        ProjectSession(ProjectSession &&) = delete;
        ProjectSession &operator=(ProjectSession &&) = delete;

        void onBeginFrame() override;
        void onEndFrame() override;
        void onPreUpdate() override;
        void onUpdate(TimeMs dt) override;
        void onPreDraw() override;
        void onDraw() override;
        void onPostDraw() override;
        void onInit() override;
        void onDestroy() override;

        [[nodiscard]] ProjectTx tx(std::string name, TxOpts opts = {});

        template <typename Build>
        [[nodiscard]] TxResult
        run(std::string name, Build &&build, TxOpts opts = {}) {
            auto edit = tx(std::move(name), opts);
            using Ret = std::invoke_result_t<Build, ProjectTx &>;
            if constexpr (std::same_as<Ret, Status>) {
                auto status = std::invoke(std::forward<Build>(build), edit);
                if (!status) {
                    edit.cancel();
                    return {.status = std::move(status)};
                }
            } else {
                static_assert(std::same_as<Ret, void>,
                              "transaction builders must return Status or "
                              "void");
                std::invoke(std::forward<Build>(build), edit);
            }
            return edit.commit();
        }

        [[nodiscard]] TxResult
        addComp(std::shared_ptr<Canvas::SceneComponent> comp,
                std::vector<std::shared_ptr<Canvas::SceneComponent>> kids = {},
                UUID scene = UUID::null);
        [[nodiscard]] TxResult
        trackAdd(std::shared_ptr<Canvas::SceneComponent> comp,
                 std::vector<std::shared_ptr<Canvas::SceneComponent>> kids = {},
                 UUID scene = UUID::null);
        [[nodiscard]] ValResult<UUID>
        addComp(std::shared_ptr<SimEngine::Drivers::CompDef> def,
                glm::vec2 pos = {},
                UUID scene = UUID::null);
        [[nodiscard]] TxResult rmComp(std::vector<UUID> ids,
                                      UUID scene = UUID::null);
        [[nodiscard]] TxResult rmComp(UUID id, UUID scene = UUID::null);
        [[nodiscard]] TxResult
        addSlot(std::shared_ptr<Canvas::SceneComponent> slot,
                UUID parent,
                UUID scene = UUID::null);
        [[nodiscard]] TxResult
        addConn(std::shared_ptr<Canvas::SceneComponent> conn,
                UUID scene = UUID::null);
        [[nodiscard]] TxResult
        trackConn(std::shared_ptr<Canvas::SceneComponent> conn,
                  UUID scene = UUID::null);
        [[nodiscard]] TxResult moveComp(UUID id,
                                        glm::vec3 pos,
                                        UUID scene = UUID::null,
                                        bool schematic = false);
        [[nodiscard]] TxResult trackMove(UUID id,
                                         glm::vec3 from,
                                         glm::vec3 to,
                                         UUID scene = UUID::null,
                                         bool schematic = false);
        [[nodiscard]] TxResult
        parentComp(UUID id, UUID parent, UUID scene = UUID::null);
        [[nodiscard]] TxResult
        trackParent(UUID id, UUID from, UUID to, UUID scene = UUID::null);
        [[nodiscard]] TxResult
        nameComp(UUID id, std::string name, UUID scene = UUID::null);
        [[nodiscard]] TxResult setComp(UUID id,
                                      Json::Value data,
                                      UUID scene = UUID::null,
                                      std::string key = {});
        [[nodiscard]] TxResult trackComp(UUID id,
                                        Json::Value from,
                                        Json::Value to,
                                        UUID scene = UUID::null,
                                        std::string key = {});
        [[nodiscard]] TxResult setName(std::string name);

        [[nodiscard]] TxResult undo();
        [[nodiscard]] TxResult redo();
        [[nodiscard]] bool canUndo() const;
        [[nodiscard]] bool canRedo() const;
        [[nodiscard]] bool dirty() const;
        [[nodiscard]] bool busy() const;
        [[nodiscard]] bool faulted() const;
        [[nodiscard]] SessView view() const;
        void clearHist();
        [[nodiscard]] Status recover();

        [[nodiscard]] Status newProj(std::string name = "Unnamed");
        [[nodiscard]] Status save();
        [[nodiscard]] Status saveAs(const std::filesystem::path &path);
        [[nodiscard]] Status load(const std::filesystem::path &path);

        [[nodiscard]] ProjectDoc &doc();
        [[nodiscard]] const ProjectDoc &doc() const;
        [[nodiscard]] SceneDriver &scenes();
        [[nodiscard]] const SceneDriver &scenes() const;
        [[nodiscard]] SimEngine::SimulationEngine &sim();
        [[nodiscard]] const SimEngine::SimulationEngine &sim() const;

        void setHooks(std::shared_ptr<const Edit::Hooks> hooks);
        [[nodiscard]] std::shared_ptr<const Edit::Hooks> hooks() const;

      private:
        friend class ProjectSessionStep;
        friend class ProjectTx;
        class Impl;

        [[nodiscard]] TxResult commit(ProjectTx &tx);
        [[nodiscard]] UUID sceneId(UUID id) const;

        std::shared_ptr<SceneDriver> m_scenes;
        std::shared_ptr<SimEngine::SimulationEngine> m_sim;
        std::unique_ptr<ProjectDoc> m_doc;
        std::unique_ptr<Impl> m;
    };
} // namespace Bess
