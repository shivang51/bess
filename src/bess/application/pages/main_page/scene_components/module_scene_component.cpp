#include "module_scene_component.h"
#include "common/bess_uuid.h"
#include "pages/main_page/main_page.h"
#include "pages/main_page/scene_components/input_scene_component.h"
#include "scene/scene_state/scene_state.h"
#include "scene_state/components/styles/comp_style.h"
#include "scene_state/components/styles/sim_comp_style.h"
#include "settings/viewport_theme.h"

namespace Bess::Canvas {
    void ModuleSceneComponent::onAttach(SceneState &state) {
    }

    std::vector<UUID> ModuleSceneComponent::cleanup(SceneState &state, UUID caller) {
        return {};
    }

    void ModuleSceneComponent::onSelect() {
    }

    std::shared_ptr<ModuleSceneComponent> ModuleSceneComponent::fromNet(const UUID &netId,
                                                                        const std::string &name) {

        auto &mainPageState = Pages::MainPage::getInstance()->getState();
        auto &sceneDriver = mainPageState.getSceneDriver();
        auto &sceneState = sceneDriver->getState();
        auto &netCompMap = mainPageState.getNetIdToCompMap(sceneDriver->getSceneId());

        if (!netCompMap.contains(netId) || netCompMap[netId].empty()) {
            BESS_WARN("[ModuleSceneComponent] No components found for netId {}, cannot create module component.",
                      (uint64_t)netId);
            return nullptr;
        }

        const auto &compIds = netCompMap.at(netId);

        auto newScene = sceneDriver.createNewScene();

        auto &newSceneState = newScene->getState();

        auto moduleComp = std::make_shared<ModuleSceneComponent>();
        moduleComp->setSceneId(newScene->getSceneId());
        moduleComp->setName(name);
        auto &style = moduleComp->getStyle();

        style.color = ViewportTheme::colors.componentBG;
        style.headerColor = ViewportTheme::colors.componentBG;
        style.borderRadius = glm::vec4(6.f);
        style.borderColor = ViewportTheme::colors.componentBorder;
        style.borderSize = glm::vec4(1.f);
        style.color = ViewportTheme::colors.componentBG;

        std::vector<std::shared_ptr<SceneComponent>> compsToMove;

        // move components and their dependants to new scene
        {
            std::unordered_set<UUID> visited;
            std::function<void(const UUID &)> collect = [&](const UUID &uuid) {
                if (visited.contains(uuid))
                    return;
                auto comp = sceneState.getComponentByUuid(uuid);
                if (!comp)
                    return;

                visited.insert(uuid);
                for (const auto &depUuid : comp->getDependants(sceneState)) {
                    collect(depUuid);
                }
                compsToMove.push_back(comp);
            };

            for (const auto &startUuid : compIds) {
                collect(startUuid);
            }
        }

        bool inputDone = false;
        bool outputDone = false;

        // moving i.e. removing from the active scene to the new scene
        for (const auto &comp : compsToMove) {
            sceneState.removeFromMap(comp->getUuid());
            newSceneState.addComponent(comp, false, false);

            if (comp->getTypeName() == InputSceneComponent::getStaticTypeName()) {
                comp->cast<InputSceneComponent>()->setIsModuleInput(true);
                moduleComp->setAssociatedInp(comp->getUuid());
                inputDone = true;
            }
        }

        return moduleComp;
    }

    void ModuleSceneComponent::draw(SceneDrawContext &context) {
        drawBackground(context);

        const auto &state = *context.sceneState;
        if (m_associatedInp != UUID::null) {
            const auto &inpComp = state.getComponentByUuid<InputSceneComponent>(m_associatedInp);
            for (const auto &childId : inpComp->getOutputSlots()) {
            }
        }

        if (m_associatedOut != UUID::null) {
        }
    }

    void ModuleSceneComponent::update(TimeMs frameTime, SceneState &state) {
        if (m_isScaleDirty) {
            setScale(calculateScale(state));
            m_isScaleDirty = false;
        }
    }

    void ModuleSceneComponent::drawBackground(SceneDrawContext &context) {

        const auto pickingId = PickingId{m_runtimeId, 0};
        Renderer::QuadRenderProperties props;
        props.angle = m_transform.angle;
        props.borderRadius = m_style.borderRadius;
        props.borderSize = m_style.borderSize;
        props.borderColor = m_isSelected
                                ? ViewportTheme::colors.selectedComp
                                : m_style.borderColor;
        props.isMica = true;
        props.shadow = {
            .enabled = true,
            .offset = glm::vec2(0.f, 0.f),
            .scale = glm::vec2(1.701f, 1.701f),
            .color = glm::vec4(1.f),
        };

        context.materialRenderer->drawQuad(m_transform.position,
                                           m_transform.scale,
                                           m_style.color,
                                           pickingId,
                                           props);

        // header
        props = {};
        props.angle = m_transform.angle;
        props.borderSize = glm::vec4(0.f);
        props.borderRadius = glm::vec4(0,
                                       0,
                                       m_style.borderRadius.x - m_style.borderSize.x,
                                       m_style.borderRadius.y - m_style.borderSize.y);
        props.isMica = true;

        const float headerHeight = Styles::componentStyles.headerHeight;
        const auto headerPos = glm::vec3(m_transform.position.x,
                                         m_transform.position.y - (m_transform.scale.y / 2.f) + (headerHeight / 2.f),
                                         m_transform.position.z + 0.0004f);
        context.materialRenderer->drawQuad(headerPos,
                                           glm::vec2(m_transform.scale.x - m_style.borderSize.w - m_style.borderSize.y,
                                                     headerHeight - m_style.borderSize.x - m_style.borderSize.z),
                                           m_style.headerColor,
                                           pickingId,
                                           props);

        const auto textPos = glm::vec3(m_transform.position.x - (m_transform.scale.x / 2.f) + Styles::componentStyles.paddingX,
                                       headerPos.y + Styles::simCompStyles.paddingY,
                                       m_transform.position.z + 0.0005f);
        // component name
        context.materialRenderer->drawText(m_name,
                                           textPos,
                                           Styles::simCompStyles.headerFontSize,
                                           ViewportTheme::colors.text,
                                           pickingId,
                                           m_transform.angle);
    }

    glm::vec2 ModuleSceneComponent::calculateScale(SceneState &state) {
        auto scale = SceneComponent::calculateScale(state);
        size_t inpCount = 0, outCount = 0;
        if (m_associatedInp != UUID::null) {
            // TODO+FIXME: Figure this out, associtatedInp is in other scene right now
            auto inpComp = state.getComponentByUuid<InputSceneComponent>(m_associatedInp);
            inpCount = inpComp->getOutputSlotsCount();
        }

        if (m_associatedOut != UUID::null) {
            auto outComp = state.getComponentByUuid<SimulationSceneComponent>(m_associatedOut);
            outCount = outComp->getInputSlotsCount();
        }

        const size_t maxRows = std::max(inpCount, outCount);
        const float height = ((float)maxRows * Styles::SCHEMATIC_VIEW_PIN_ROW_SIZE) +
                             Styles::componentStyles.headerHeight;
        scale.y = std::max(scale.y, height);

        return scale;
    }
} // namespace Bess::Canvas
