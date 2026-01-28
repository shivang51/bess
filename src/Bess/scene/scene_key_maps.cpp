#include "pages/main_page/main_page_state.h"
#include "scene.h"
#include "ui/ui_main/component_explorer.h"
#include "ui/ui_main/project_explorer.h"
#include <GLFW/glfw3.h>
#include <ranges>

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
        }
        ELIF_KEY_PRESSED(KEY(F)) { // `f` focus camera on selected component
            const auto &selectedComps = m_state.getSelectedComponents() | std::views::keys;

            if (!selectedComps.empty()) {
                const auto &comp = m_state.getComponentByUuid(*selectedComps.begin());
                m_camera->focusAtPoint(glm::vec2(comp->getAbsolutePosition(m_state)), true);
            }
        }
        ELIF_KEY_PRESSED(KEY(TAB)) {
            toggleSchematicView();
        }
    }
} // namespace Bess::Canvas
