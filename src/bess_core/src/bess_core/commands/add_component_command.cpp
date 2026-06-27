#include "bess_core/commands/add_component_command.h"

#include "bess_core/scene/scene_state/scene_state.h"

namespace Bess::Cmd {
    AddSceneComponentCmd::AddSceneComponentCmd() {
        m_name = "AddComponentCmd";
    }

    AddSceneComponentCmd::AddSceneComponentCmd(
        std::shared_ptr<Canvas::SceneComponent> component)
        : m_component(std::move(component)) {
        m_name = "AddComponentCmd";
    }

    AddSceneComponentCmd::AddSceneComponentCmd(
        std::shared_ptr<Canvas::SceneComponent> component,
        std::vector<std::shared_ptr<Canvas::SceneComponent>> childComponents)
        : m_component(std::move(component)),
          m_childComponents(std::move(childComponents)) {
        m_name = "AddComponentCmd";
    }

    bool AddSceneComponentCmd::execute(const CommandContext &context) {
        return addToScene(context, true);
    }

    void AddSceneComponentCmd::undo(const CommandContext &context) {
        const auto commandContext = makeContext(context);
        if (!commandContext.scene || !m_component) {
            return;
        }

        removeSceneComponentWithHooks(commandContext.scene,
                                      m_component,
                                      UUID::master,
                                      commandContext.componentHooks);
    }

    void AddSceneComponentCmd::redo(const CommandContext &context) {
        addToScene(context, false);
    }

    const std::shared_ptr<Canvas::SceneComponent> &
    AddSceneComponentCmd::getComponent() const {
        return m_component;
    }

    bool AddSceneComponentCmd::addToScene(const CommandContext &context,
                                          bool setZ) {
        const auto commandContext = makeContext(context);
        if (!commandContext.scene || !m_component) {
            return false;
        }

        const bool deferParentAttach = !m_childComponents.empty();
        if (!addSceneComponentWithHooks(commandContext.scene,
                                        m_component,
                                        {.setZ = setZ,
                                         .triggerAttach = !deferParentAttach,
                                         .dispatchEvent = true},
                                        commandContext.componentHooks)) {
            return false;
        }

        auto &sceneState = commandContext.scene->getState();
        for (const auto &childComponent : m_childComponents) {
            if (!childComponent) {
                continue;
            }

            if (!addSceneComponentWithHooks(commandContext.scene,
                                            childComponent,
                                            {.setZ = false,
                                             .triggerAttach = true,
                                             .dispatchEvent = true},
                                            commandContext.componentHooks)) {
                return false;
            }

            sceneState.attachChild(m_component->getUuid(),
                                   childComponent->getUuid());
        }

        if (deferParentAttach &&
            sceneState.isComponentValid(m_component->getUuid())) {
            m_component->onAttach(sceneState);
        }

        return true;
    }
} // namespace Bess::Cmd
