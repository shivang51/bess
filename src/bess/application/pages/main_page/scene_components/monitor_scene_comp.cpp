#include "pages/main_page/scene_components/monitor_scene_comp.h"
#include "bess_core/project_context.h"
#include "bess_core/renderer/renderer_2d.h"
#include "bess_core/renderer/renderer_types.h"
#include "bess_core/scene/scene_state/components/styles/comp_style.h"
#include "ext/vector_float2.hpp"
#include "ext/vector_float3.hpp"
#include "pages/main_page/scene_components/sim_scene_component.h"
#include "pages/main_page/scene_components/slot_scene_component.h"
#include "simulation_engine.h"

namespace Bess::Canvas {
    MonitorSceneComp::MonitorSceneComp() {
        m_name = "Monitor Node";
        m_transform.scale = {300.f, 150.f};
    }

    std::vector<std::shared_ptr<SceneComponent>>
    MonitorSceneComp::clone(const SceneState &sceneState) const {
        (void)sceneState;
        auto clonedComponent = std::make_shared<MonitorSceneComp>(*this);
        prepareClone(*clonedComponent);
        clonedComponent->m_probedSlots.clear();
        clonedComponent->m_probeData.clear();
        return {clonedComponent};
    }

    void MonitorSceneComp::draw(SceneDrawContext &context) {
        const auto pickingId = PickingId{m_runtimeId, 0};

        context.renderer->drawQuad({
            .position = m_transform.position,
            .size = m_transform.scale,
            .zIndex = m_transform.position.z,
            .color = ViewportTheme::colors.componentBG,
            .id = pickingId,
            .radius = Styles::componentStyles.borderRadius,
            .shadow = {.enabled = true},
        });

        const glm::vec2 &topLeft =
            m_transform.position -
            glm::vec3(
                m_transform.scale.x / 2.f, m_transform.scale.y / 2.f, 0.f) +
            glm::vec3(Styles::componentStyles.paddingX,
                      Styles::componentStyles.paddingY * 2.f,
                      0.f);

        const auto textYOffset = context.renderer->textCenterOffsetY(
            m_name, {.fontSize = Styles::componentStyles.headerFontSize});

        context.renderer->drawFont(
            m_name,
            {
                .position = topLeft + glm::vec2(0.f, textYOffset),
                .fontSize = Styles::componentStyles.headerFontSize,
                .zIndex = m_transform.position.z + 0.0001f,
                .id = pickingId,
            });

        plotProbedData(context);

        const glm::vec3 slotStartPos = {
            topLeft.x, m_transform.position.y, 0.0001f};
        for (const auto &slotUuid : m_probedSlots) {
            const auto &comp =
                context.sceneState->getComponentByUuid<SlotSceneComponent>(
                    slotUuid);
            if (!comp)
                continue;

            const auto slotPos = comp->getAbsolutePosition(
                *context.sceneState, context.isSchematicMode);

            context.renderer->beginPath({
                .strokeColor = ViewportTheme::colors.wire,
                .renderFill = false,
                .zIndex = m_transform.position.z + 0.0001f,
                .id = PickingId{m_runtimeId, 1},
                .closePath = false,
            });
            context.renderer->pathMoveTo(slotStartPos);
            context.renderer->pathCubicTo(
                glm::vec2(slotPos.x, slotPos.y),
                glm::vec2((slotStartPos.x + slotPos.x) / 2.f, slotStartPos.y),
                slotPos,
                1.f);
            context.renderer->endPath();
        }
    }

    void MonitorSceneComp::plotProbedData(SceneDrawContext &context) {
        auto startPos = m_transform.position;
        startPos.x +=
            (-m_transform.scale.x / 2.f) + Styles::componentStyles.paddingX;
        startPos.z += 0.0001f;

        for (const auto &[slotUuid, data] : m_probeData) {
            if (data.empty())
                continue;

            auto color = Core::Renderer::Color::fromHex((uint32_t)slotUuid);
            // color.a = 1.f;

            context.renderer->beginPath({
                .strokeColor = color,
                .renderFill = false,
                .zIndex = m_transform.position.z + 0.0001f,
                .closePath = false,
            });

            auto prev = data.front();
            auto timeSec = std::chrono::duration<float>(prev.first).count();
            const auto offset =
                glm::vec2(startPos) - glm::vec2(timeSec * 10, 25.f);

            for (size_t i = 0; i < data.size(); ++i) {
                auto &[time, voltage] = data[i];
                timeSec = std::chrono::duration<float>(time).count();
                auto pos = glm::vec2(timeSec * 10, voltage * 10) + offset;

                if (i == 0) {
                    context.renderer->pathMoveTo(pos);
                } else {
                    context.renderer->pathLineTo(pos, 1.f);
                }

                // For drawing a square wave
                if (i + 1 < data.size()) {
                    auto &[_, voltageN] = data[i + 1];
                    auto nextPos =
                        glm::vec2(timeSec * 10, voltageN * 10) + offset;
                    context.renderer->pathLineTo(nextPos, 1.f);
                }
            }

            context.renderer->endPath();
        }
    }

    void MonitorSceneComp::update(TimeMs frameTime, SceneState &state) {
    }

    void MonitorSceneComp::onMouseButton(const Events::MouseButtonEvent &e) {
        if (e.action == Events::MouseClickAction::press &&
            e.button == Events::MouseButton::left) {
            const auto &connStartSlot = e.sceneState->getConnectionStartSlot();
            if (connStartSlot != UUID::null) {
                const auto &comp =
                    e.sceneState->getComponentByUuid<SlotSceneComponent>(
                        connStartSlot);
                if (comp && comp->getType() == SceneComponentType::slot &&
                    comp->getSlotType() != SlotType::inputsResize &&
                    comp->getSlotType() != SlotType::outputsResize) {
                    addSlotProbe(*e.sceneState,
                                 e.sceneState->getConnectionStartSlot());
                    e.sceneState->setConnectionStartSlot(UUID::null);
                }
            }
        }
    }

    void MonitorSceneComp::addSlotProbe(const SceneState &sceneState,
                                        const UUID &slotUuid) {
        if (m_probedSlots.contains(slotUuid)) {
            return;
        }

        m_probedSlots.insert(slotUuid);
        subscribeToSlot(sceneState, slotUuid);
    }

    void MonitorSceneComp::removeSlotProbe(const SceneState &sceneState,
                                           const UUID &slotUuid) {
        if (!m_probedSlots.contains(slotUuid)) {
            return;
        }

        m_probedSlots.erase(slotUuid);
        unsubscribeFromSlot(sceneState, slotUuid);
    }

    void MonitorSceneComp::subscribeToSlot(const SceneState &sceneState,
                                           const UUID &slotUuid) {
        const auto &comp =
            sceneState.getComponentByUuid<SlotSceneComponent>(slotUuid);
        if (!comp)
            return;

        const auto &simId = sceneState
                                .getComponentByUuid<SimulationSceneComponent>(
                                    comp->getParentComponent())
                                ->getSimEngineId();

        auto &appCtx = Bess::GAppContext::getInstance();
        auto projectCtx = appCtx.getSubSystem<Bess::ProjectContext>();
        const auto &simEngine = projectCtx->getSimEngine();
        const auto &digComp =
            simEngine.getComponent<SimEngine::Drivers::Digital::DigSimComp>(
                simId);

        digComp->addOnStateChangeCB(
            m_uuid,
            [this, slotUuid, slotComp = comp](
                const std::vector<SimEngine::SlotState> &inputStates,
                const std::vector<SimEngine::SlotState> &outputStates) {
                SimEngine::SlotState slotState;

                if (slotComp->isInputSlot()) {
                    slotState = inputStates[slotComp->getIndex()];
                } else {
                    slotState = outputStates[slotComp->getIndex()];
                }

                auto &probeData = m_probeData[slotUuid];

                probeData.emplace_back(slotState.lastChangeTime,
                                       slotState.voltage);

                if (probeData.size() > 100) {
                    probeData.erase(probeData.begin());
                }
            });
    }

    void MonitorSceneComp::unsubscribeFromSlot(const SceneState &sceneState,
                                               const UUID &slotUuid) {
        if (slotUuid == UUID::null)
            return;

        const auto &comp =
            sceneState.getComponentByUuid<SlotSceneComponent>(slotUuid);
        if (!comp)
            return;

        const auto &simId = sceneState
                                .getComponentByUuid<SimulationSceneComponent>(
                                    comp->getParentComponent())
                                ->getSimEngineId();

        auto &appCtx = Bess::GAppContext::getInstance();
        auto projectCtx = appCtx.getSubSystem<Bess::ProjectContext>();
        const auto &simEngine = projectCtx->getSimEngine();
        const auto &digComp =
            simEngine.getComponent<SimEngine::Drivers::Digital::DigSimComp>(
                simId);
        digComp->removeOnStateChangeCB(m_uuid);

        m_probeData.erase(slotUuid);
    }

} // namespace Bess::Canvas
