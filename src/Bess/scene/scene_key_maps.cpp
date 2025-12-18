#include "pages/main_page/main_page_state.h"
#include "scene.h"
#include "ui/ui_main/component_explorer.h"
#include "ui/ui_main/project_explorer.h"
#include <GLFW/glfw3.h>

namespace Bess::Canvas {

#define IF_LEFT_CTRL_PRESSED \
    if (mainPageState->isKeyPressed(GLFW_KEY_LEFT_CONTROL))

#define IF_LEFT_SHIFT_PRESSED \
    if (mainPageState->isKeyPressed(GLFW_KEY_LEFT_SHIFT))

#define IF_KEY_PRESSED(key) \
    if (mainPageState->isKeyPressed(key))

#define ELIF_KEY_PRESSED(key) \
    else if (mainPageState->isKeyPressed(key))

#define KEY(key) GLFW_KEY_##key

    void Scene::handleKeyboardShortcuts() {
        const auto mainPageState = Pages::MainPageState::getInstance();

        IF_LEFT_CTRL_PRESSED {
            IF_LEFT_SHIFT_PRESSED {
                IF_KEY_PRESSED(KEY(Z)) // ctrl-shift-z redo
                m_cmdManager.redo();
            }
            ELIF_KEY_PRESSED(KEY(A)) // ctrl-a select all components
            selectAllEntities();
            ELIF_KEY_PRESSED(KEY(C)) // ctrl-c copy selected components
            copySelectedComponents();
            ELIF_KEY_PRESSED(KEY(V)) // ctrl-v generate copied components
            generateCopiedComponents();
            ELIF_KEY_PRESSED(KEY(Z)) // ctrl-z undo
            m_cmdManager.undo();
            ELIF_KEY_PRESSED(KEY(G)) // ctrl-g group selected components
            {
                UI::ProjectExplorer::groupSelectedNodes();
            }
        }
        else IF_LEFT_SHIFT_PRESSED {
            IF_KEY_PRESSED(KEY(A)) { // shift-a toggle component explorer
                UI::ComponentExplorer::isShown = !UI::ComponentExplorer::isShown;
            }
        }
        ELIF_KEY_PRESSED(KEY(DELETE)) { // delete selected component(s)
            deleteSelectedSceneEntities();
            // const auto view = m_registry.view<Components::IdComponent, Components::SelectedComponent>();
            //
            // std::vector<UUID> entitesToDel = {};
            // std::vector<entt::entity> connEntitesToDel = {};
            // for (const auto &entt : view) {
            //     if (!m_registry.valid(entt))
            //         continue;
            //
            //     if (m_registry.all_of<Components::ConnectionComponent>(entt)) {
            //         connEntitesToDel.emplace_back(entt);
            //     } else {
            //         entitesToDel.emplace_back(getUuidOfEntity(entt));
            //     }
            // }
            //
            // auto _ = m_cmdManager.execute<Commands::DeleteCompCommand, std::string>(entitesToDel);
            //
            // std::vector<UUID> connToDel = {};
            // for (const auto ent : connEntitesToDel) {
            //     if (!m_registry.valid(ent))
            //         continue;
            //
            //     connToDel.emplace_back(getUuidOfEntity(ent));
            // }
            //
            // _ = m_cmdManager.execute<Commands::DelConnectionCommand, std::string>(connToDel);
        }
        ELIF_KEY_PRESSED(KEY(F)) { // `f` focus camera on selected component
            const auto view = m_registry.view<Components::IdComponent,
                                              Components::SelectedComponent,
                                              Components::TransformComponent>();

            // pick the first one to focus. if many are selected
            for (const auto &ent : view) {
                const auto &transform = view.get<Components::TransformComponent>(ent);
                m_camera->focusAtPoint(glm::vec2(transform.position), true);
                break;
            }
        }
        ELIF_KEY_PRESSED(KEY(TAB)) {
            toggleSchematicView();
        }
    }
} // namespace Bess::Canvas
