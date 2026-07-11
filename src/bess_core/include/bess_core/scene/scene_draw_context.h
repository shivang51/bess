#pragma once

#include "bess_core/renderer/renderer_types.h"
#include "bess_core/scene/scene_ui/layout.h"
#include "bess_core/style/bess_theme.h"
#include "bess_core/viewport.h"
#include "common/bess_assert.h"
#include "simulation_engine.h"
#include <memory>

namespace Bess {

    class Camera;

    namespace Core::Renderer {
        class IRenderer2D;
    } // namespace Core::Renderer

    namespace Canvas {
        class SceneState;

        namespace SceneWidgets {
            struct SceneWidgetsState;
        } // namespace SceneWidgets
    } // namespace Canvas

    struct SimDrawCache {
      public:
        void setSimEngine(
            const std::shared_ptr<SimEngine::SimulationEngine> &engine) {
            m_engine = engine;
        }

        // sim comp id
        const SimEngine::ComponentState &getCompState(const UUID &compId) {
            auto itr = m_states.find(compId);
            if (itr != m_states.end()) {
                return itr->second;
            }

            BESS_ASSERT(m_engine != nullptr, "Simulation engine is not set");

            auto state = m_engine->getComponentState(compId);
            m_states[compId] = state;
            return m_states[compId];
        }

        bool isPortConnected(const SimEngine::PortRef &port) {
            const auto &state = getCompState(port.componentId);

            if (port.isInput()) {
                return (size_t)port.index < state.inputConnected.size() &&
                       state.inputConnected[port.index];
            }

            return (size_t)port.index < state.outputConnected.size() &&
                   state.outputConnected[port.index];
        }

        SimEngine::PortState getPortState(const SimEngine::PortRef &port) {
            const auto &state = getCompState(port.componentId);

            if (port.isInput()) {
                BESS_ASSERT((size_t)port.index < state.inputStates.size(),
                            "Input port index out of bounds");
                return state.inputStates[port.index];
            }

            BESS_ASSERT((size_t)port.index < state.outputStates.size(),
                        "Output port index out of bounds");
            return state.outputStates[port.index];
        }

        void clear() {
            m_states.clear();
        }

      private:
        std::shared_ptr<SimEngine::SimulationEngine> m_engine = nullptr;
        HashMap<UUID, SimEngine::ComponentState> m_states;
    };

    struct SceneDrawContext {
        Bess::Canvas::SceneState *sceneState = nullptr;
        std::shared_ptr<Core::Renderer::IRenderer2D> renderer = nullptr;
        std::shared_ptr<Camera> camera = nullptr;
        Core::Renderer::RenderTransformMode transformMode =
            Core::Renderer::RenderTransformMode::Camera;
        size_t viewportId = 0;
        bool isSchematicMode = false;
        Canvas::SceneWidgets::SceneWidgetsState *sceneWidgetsState = nullptr;
        SimDrawCache *simDrawCache = nullptr;
        std::shared_ptr<Core::Viewport::ViewportContext> viewportCtx = nullptr;
    };

    struct SceneUIPrepareCtx {
        Bess::Canvas::SceneState *sceneState;
        std::shared_ptr<Core::Renderer::IRenderer2D> renderer;
        Canvas::UI::UINode *parentNode = nullptr;
        std::shared_ptr<Bess::Core::Style::BessTheme> theme = nullptr;
    };
} // namespace Bess
