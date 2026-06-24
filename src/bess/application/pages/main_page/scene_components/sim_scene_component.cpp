#include "sim_scene_component.h"
#include "bess_core/connection_service.h"
#include "bess_core/g_app_context.h"
#include "bess_core/project_context.h"
#include "bess_core/renderer/renderer_2d.h"
#include "bess_core/renderer/renderer_types.h"
#include "bess_core/scene/scene_draw_context.h"
#include "bess_core/scene/scene_draw_helpers.h"
#include "bess_core/scene/scene_state/components/scene_component.h"
#include "bess_core/scene/scene_state/components/styles/comp_style.h"
#include "bess_core/scene/scene_state/components/styles/sim_comp_style.h"
#include "bess_core/scene/scene_state/scene_state.h"
#include "bess_core/settings/viewport_theme.h"
#include "common/bess_uuid.h"
#include "input_scene_component.h"
#include "simulation_engine.h"
#include "slot_scene_component.h"
#include "sub_systems/renderer_context.h"
#include "ui/icons/FontAwesomeIcons_Remapped.h"
#include "ui/ui_main/ui_main.h"
#include "ui/widgets/m_widgets.h"

#include <algorithm>
#include <cstdint>
#include <unordered_set>

namespace Icons = Bess::UI::Icons;
namespace Widgets = Bess::UI::Widgets;

namespace Bess::Canvas {
    uint32_t SimulationSceneComponent::s_nodeShader = 0;
    size_t SimulationSceneComponent::s_instanceCount = 0;

    constexpr float SNAP_AMOUNT = 2.f;

    SimulationSceneComponent::SimulationSceneComponent() {
        m_icon = Icons::FontAwesomeIcons::FA_MICROCHIP;

        if (s_nodeShader == 0) {
            const auto &appCtx = Bess::GAppContext::getInstance();
            const auto &rendererCtx =
                appCtx.getSubSystem<Bess::RendererContext>();

            Core::Renderer::CustomQuadShaderDesc desc;

            desc.label = "MicaQuadShader";
            desc.fragmentSource = R"(
fn cornerRadiusForPoint(p: vec2f, radii: vec4f) -> f32 {
    var radius = radii.w;
    if (p.x < 0.0 && p.y < 0.0) {
        radius = radii.x;
    } else if (p.x >= 0.0 && p.y < 0.0) {
        radius = radii.y;
    } else if (p.x >= 0.0 && p.y >= 0.0) {
        radius = radii.z;
    }
    return radius;
}

fn sdRoundedRect(p: vec2f, halfSize: vec2f, radii: vec4f) -> f32 {
    let maxRadius = max(min(halfSize.x, halfSize.y), 0.0);
    let clampedRadii = clamp(radii, vec4f(0.0), vec4f(maxRadius));
    let radius = cornerRadiusForPoint(p, clampedRadii);
    let innerHalfSize = max(halfSize - vec2f(radius), vec2f(0.0));
    let d = abs(p) - innerHalfSize;
    return length(max(d, vec2f(0.0))) + min(max(d.x, d.y), 0.0) - radius;
}

fn borderWidthForPoint(p: vec2f, halfSize: vec2f, radii: vec4f, borderSize: vec4f) -> f32 {
    let maxRadius = max(min(halfSize.x, halfSize.y), 0.0);
    let clampedRadii = clamp(radii, vec4f(0.0), vec4f(maxRadius));
    let radius = cornerRadiusForPoint(p, clampedRadii);
    let inCorner = radius > 0.0 &&
                   abs(p.x) > halfSize.x - radius &&
                   abs(p.y) > halfSize.y - radius;

    if (inCorner) {
        if (p.x < 0.0 && p.y < 0.0) {
            return min(borderSize.x, borderSize.w);
        } else if (p.x >= 0.0 && p.y < 0.0) {
            return min(borderSize.x, borderSize.y);
        } else if (p.x >= 0.0 && p.y >= 0.0) {
            return min(borderSize.z, borderSize.y);
        }
        return min(borderSize.z, borderSize.w);
    }

    let distToTop = p.y + halfSize.y;
    let distToRight = halfSize.x - p.x;
    let distToBottom = halfSize.y - p.y;
    let distToLeft = p.x + halfSize.x;
    let nearestHorizontal = min(distToLeft, distToRight);
    let nearestVertical = min(distToTop, distToBottom);

    if (nearestVertical <= nearestHorizontal) {
        if (distToTop <= distToBottom) {
            return borderSize.x;
        }
        return borderSize.z;
    }
    if (distToRight <= distToLeft) {
        return borderSize.y;
    }
    return borderSize.w;
}

fn aaWidth(fw: vec2f) -> f32 {
    return max(length(fw) * 0.5, 0.000001);
}

fn shadeTintedGlass(base: vec4f, localUv: vec2f, style: vec4f, headerColor: vec4f, isDark: bool) -> vec4f {
    let uv = clamp(localUv, vec2f(0.0), vec2f(1.0));
    let headerHeight = clamp(style.x, 0.0, 1.0);
    
    let centeredUv = abs((uv * 2.0) - vec2f(1.0));
    let edge = smoothstep(0.15, 1.0, max(centeredUv.x, centeredUv.y));
    let cornerGlow = smoothstep(0.20, 0.6, length(centeredUv));

    // --- PROCEDURAL DESIGN REGIME TOKENS ---
    var rimTint = vec3f(0.0);
    var headerBase = vec3f(0.0);
    var headerColorMul = 0.0;
    var headerFalloffMul = 0.0;
    var bodyBase = vec3f(0.0);
    var bodyColorMul = 0.0;
    var bodyBleed = 0.0;
    var bodyShadow = 0.0;
    var alphaMin = 0.0;
    var alphaMax = 0.0;

    if (isDark) {
        rimTint           = vec3f(0.03); // Lightweight interior additive edge light
        headerBase        = vec3f(0.010, 0.012, 0.014); // Deep slate canvas floor
        headerColorMul    = 0.48;
        headerFalloffMul  = 0.35; // Rich dark gradient attenuation
        bodyBase          = max(base.rgb * 0.35, vec3f(0.018, 0.020, 0.024));
        bodyColorMul      = 0.55;
        bodyBleed         = 0.06;
        bodyShadow        = 0.008; // Bottom structural anchor shade
        alphaMin          = 0.62;
        alphaMax          = 0.74;
    } else {
        rimTint           = vec3f(-0.06); // Negative coefficients translate to structural drop-vignettes
        headerBase        = vec3f(0.95, 0.95, 0.96); // Clean, luminous bright background base
        headerColorMul    = 0.88; // Keep headers vibrant and punchy
        headerFalloffMul  = 0.92; // Extremely soft top-down shadow blend
        bodyBase          = min(base.rgb * 1.02, vec3f(0.98, 0.98, 0.99)); // Crisp paper look
        bodyColorMul      = 1.00;
        bodyBleed         = 0.01; // Restrained color bleeding to prevent looking messy
        bodyShadow        = 0.025; // Enhanced grounding ambient shadow profile
        alphaMin          = 0.84; // Elevated opacity to shield underlying grids and keep text readable
        alphaMax          = 0.94;
    }

    let headerBlend = 1.0 - smoothstep(headerHeight - 0.02, headerHeight + 0.06, uv.y);
    
    // --- HEADER SEGMENT ---
    let radialCenter = vec2f(0.5, 0.0);
    let radialDist = length(uv - radialCenter);
    let headerRadialGlow = smoothstep(0.85, 0.0, radialDist); 
    
    let topEdgeLight = smoothstep(0.025, 0.0, uv.y) * smoothstep(0.02, 0.08, uv.x) * smoothstep(0.98, 0.92, uv.x);
    let headerVerticalFalloff = smoothstep(0.0, headerHeight, uv.y);
    
    var headerRgb = mix(headerBase, headerColor.rgb * headerColorMul, headerRadialGlow);
    headerRgb = mix(headerRgb, headerRgb * headerFalloffMul, headerVerticalFalloff);
    
    // Specular reflections remain crisp and clean across light and dark profiles
    let specularColor = mix(vec3f(1.0), headerColor.rgb, 0.25);
    headerRgb += specularColor * 0.45 * topEdgeLight;
    headerRgb += headerColor.rgb * 0.12 * headerRadialGlow * (1.0 - headerVerticalFalloff);

    // --- BODY SEGMENT ---
    let bodyTopGlow = smoothstep(headerHeight + 0.30, headerHeight, uv.y);
    let bodyBottomShadow = smoothstep(0.75, 1.0, uv.y);
    
    var bodyRgb = mix(bodyBase, base.rgb * bodyColorMul, 0.22); 
    bodyRgb += headerColor.rgb * bodyBleed * bodyTopGlow; 
    bodyRgb -= vec3f(bodyBottomShadow * bodyShadow);

    // --- FINAL LAYER COMPOSITING ---
    var finalRgb = mix(bodyRgb, headerRgb, headerBlend);
    finalRgb += rimTint * ((edge * 0.055) + (cornerGlow * 0.020));

    let alpha = base.a * mix(alphaMin, alphaMax, headerBlend);
    return vec4f(clamp(finalRgb, vec3f(0.0), vec3f(1.0)), alpha);
}

fn shadeQuad(in: CustomQuadFragmentInput, fw: vec2f) -> vec4f {
    let halfSize = max(in.size * 0.5, vec2f(0.0001));
    let p = in.local_pos;
    let outerDistance = sdRoundedRect(p, halfSize, vec4f(in.data0.x));
    let aa = aaWidth(fw);
    let outerMask = 1.0 - smoothstep(-aa, aa, outerDistance);

    if (outerMask < 0.001) {
        discard;
    }

    // Capture the runtime environment flag safely 
    let isDark = in.data3.y > 0.5;

    let borderSizeIn = vec4f(in.data0.z);
    let borderSize = clamp(borderSizeIn, vec4f(0.0),
                           vec4f(halfSize.y, halfSize.x, halfSize.y, halfSize.x));
    let border = max(max(borderSize.x, borderSize.y),
                     max(borderSize.z, borderSize.w));
    let borderWidth = borderWidthForPoint(p, halfSize, in.data0, borderSize);
    let borderMask = smoothstep(-borderWidth - aa, -borderWidth + aa, outerDistance);

    // FIXED: Passed runtime context down to the rendering pipeline
    var color = shadeTintedGlass(in.color, in.local_uv, in.data3, in.data1, isDark);

    if (border > 0.0) {
        color = mix(color, in.data2, borderMask);
    }
    color.a *= outerMask;
    return color;
}

fn custom_quad_fragment(in: CustomQuadFragmentInput) -> vec4f {
    let fw_local_px = fwidth(in.local_pos);
    return shadeQuad(in, fw_local_px);
}
	)";

            s_nodeShader =
                rendererCtx->getRenderer()->createCustomQuadShader(desc);
        }

        s_instanceCount++;
    }

    std::vector<std::shared_ptr<SceneComponent>>
    SimulationSceneComponent::clone(const SceneState &sceneState) const {
        auto clonedComponent =
            std::make_shared<SimulationSceneComponent>(*this);
        return cloneSimulationComponent(sceneState, clonedComponent);
    }

    void SimulationSceneComponent::update(Bess::TimeMs timeStep,
                                          SceneState &state) {

        updateScales(state);
    }

    void SimulationSceneComponent::updateScales(const SceneState &state) {
        if (m_isScaleDirty) {
            setScale(calculateScale(state));
            resetSlotPositions(state);
            m_isScaleDirty = false;
        }

        if (m_isSchematicScaleDirty) {
            calculateSchematicScale(state);
            resetSchematicPinsPositions(state);
            m_isSchematicScaleDirty = false;
        }

        if (m_isSchSlotsPosDirty) {
            resetSchematicPinsPositions(state);
            m_isSchSlotsPosDirty = false;
        }
    }

    void SimulationSceneComponent::draw(SceneDrawContext &context) {
        drawBackground(context);
        drawSlots(context);
    }

    void SimulationSceneComponent::drawBackground(SceneDrawContext &context) {
        BESS_ASSERT(s_nodeShader != 0, "Mica shader not initialized");

        const auto pickingId = PickingId{m_runtimeId, 0};

        Core::Renderer::QuadProps quadProps;
        quadProps.position = m_transform.position;
        quadProps.size = m_transform.scale;
        quadProps.color = ViewportTheme::colors.componentBG;
        quadProps.id = pickingId;
        quadProps.rotation = m_transform.angle;
        quadProps.zIndex = m_transform.position.z;
        quadProps.renderPass = Core::Renderer::QuadRenderPass::Transparent;
        quadProps.radius = Styles::componentStyles.borderRadius;
        quadProps.shadow.enabled = true;
        quadProps.shadow.offset = {0.f, 7.f};
        quadProps.shadow.blur = 18.f;
        quadProps.shadow.spread = 1.f;
        quadProps.shadow.color = Core::Renderer::Color{0.f, 0.f, 0.f, 0.28f};

        const auto &borderColor = m_isSelected
                                      ? ViewportTheme::colors.selectedComp
                                      : ViewportTheme::colors.componentBorder;
        const float headerHeight = Styles::componentStyles.headerHeight;

        context.renderer->drawCustomQuad({
            .quad = quadProps,
            .shader = s_nodeShader,
            .data =
                {
                    glm::vec4{
                        Styles::componentStyles.borderRadius.x, // border raidus
                        Styles::componentStyles.borderRadius.y,
                        Styles::componentStyles.borderSize.x, // border size
                        Styles::componentStyles.borderSize.y,
                    },
                    m_style.headerColor,
                    borderColor,
                    glm::vec4(headerHeight / m_transform.scale.y,
                              (int)ViewportTheme::isDark,
                              0.f,
                              0.f),
                },
        });

        const auto headerPosY = m_transform.position.y -
                                (m_transform.scale.y / 2.f) +
                                (headerHeight / 2.f);

        const auto textPos =
            glm::vec3(m_transform.position.x - (m_transform.scale.x / 2.f) +
                          Styles::componentStyles.paddingX,
                      headerPosY + Styles::simCompStyles.paddingY,
                      m_transform.position.z + 0.0005f);

        // component name
        SceneDraw::drawText(context,
                            std::format("{} {}", m_icon, m_name),
                            textPos,
                            Styles::simCompStyles.headerFontSize,
                            ViewportTheme::colors.text,
                            pickingId,
                            m_transform.angle);
    }

    void SimulationSceneComponent::drawSlots(SceneDrawContext &context) {
        // I know i am repeating my self here :), I have trust issues

        if (context.isSchematicMode) {
            for (const auto &childId : m_childComponents) {
                auto child = context.sceneState->getComponentByUuid(childId);
                child->drawSchematic(context);
            }
        } else {
            for (const auto &childId : m_childComponents) {
                auto child = context.sceneState->getComponentByUuid(childId);
                child->draw(context);
            }
        }
    }

    void SimulationSceneComponent::drawSchematic(SceneDrawContext &context) {
        auto &state = *context.sceneState;
        const auto &id = PickingId{m_runtimeId, 0};

        const glm::vec3 &pos =
            getAbsolutePosition(state, context.isSchematicMode);
        float x = pos.x - (m_schematicTransform.scale.x / 2.f);
        float y = pos.y - (m_schematicTransform.scale.y / 2.f);
        float x1 = x + m_schematicTransform.scale.x;
        float y1 = y + m_schematicTransform.scale.y;
        float nodeWeight = Styles::compSchematicStyles.strokeSize;
        const auto &textColor = ViewportTheme::schematicViewColors.text;
        const auto &fillColor =
            ViewportTheme::schematicViewColors.componentFill;
        const auto &strokeColor =
            ViewportTheme::schematicViewColors.componentStroke;
        SceneDraw::beginPath(
            context,
            {x, y, pos.z},
            nodeWeight,
            strokeColor,
            id,
            {.closePath = true, .renderFill = true, .fillColor = fillColor});
        SceneDraw::pathLineTo(context, {x1, y, pos.z}, nodeWeight);
        SceneDraw::pathLineTo(context, {x1, y1, pos.z}, nodeWeight);
        SceneDraw::pathLineTo(context, {x, y1, pos.z}, nodeWeight);
        SceneDraw::endPath(context);

        const auto textSize = context.renderer->measureText(
            m_name, {.fontSize = Styles::compSchematicStyles.nameFontSize});
        glm::vec3 textPos = {pos.x, y + ((y1 - y) / 2.f), pos.z + 0.0005f};
        textPos.x -= textSize.x / 2.f;
        textPos.y += Styles::simCompStyles.headerFontSize / 2.f;
        SceneDraw::drawText(context,
                            m_name,
                            textPos,
                            Styles::compSchematicStyles.nameFontSize,
                            textColor,
                            id,
                            0.f);

        drawSlots(context);

        if (m_isFirstSchematicDraw) {
            m_isFirstSchematicDraw = false;
        }
    }

    std::pair<std::vector<glm::vec3>, std::vector<glm::vec3>>
    SimulationSceneComponent::calculateSlotPositions(size_t inputCount,
                                                     size_t outputCount) const {
        const auto pScale = m_transform.scale;

        std::vector<glm::vec3> inputPositions;
        inputPositions.reserve(inputCount);
        std::vector<glm::vec3> outputPositions;
        outputPositions.reserve(outputCount);

        const auto slotRowSize = Styles::SIM_COMP_SLOT_ROW_SIZE;

        for (size_t i = 0; i < inputCount; i++) {
            auto posX = -(pScale.x / 2.f) + Styles::SIM_COMP_SLOT_DX;
            float posY = -(pScale.y / 2.f) + (slotRowSize * (float)i) +
                         (slotRowSize / 2.f);
            posY += Styles::SIM_COMP_SLOT_START_Y;
            glm::vec2 pos =
                glm::round(glm::vec2(posX, posY) / SNAP_AMOUNT) * SNAP_AMOUNT;
            inputPositions.emplace_back(pos.x, pos.y, 0.0005f);
        }

        for (size_t i = 0; i < outputCount; i++) {
            auto posX = (pScale.x / 2.f) - Styles::SIM_COMP_SLOT_DX;
            float posY = -(pScale.y / 2.f) + (slotRowSize * (float)i) +
                         (slotRowSize / 2.f);
            posY += Styles::SIM_COMP_SLOT_START_Y;
            glm::vec2 pos =
                glm::round(glm::vec2(posX, posY) / SNAP_AMOUNT) * SNAP_AMOUNT;
            outputPositions.emplace_back(pos.x, pos.y, 0.0005f);
        }

        return {inputPositions, outputPositions};
    }

    glm::vec2
    SimulationSceneComponent::calculateScale(const SceneState &state) {
        const auto labelSize = Core::Renderer::IRenderer2D::getTextRenderSize(
            m_name, {.fontSize = Styles::simCompStyles.headerFontSize});
        float width = labelSize.x + (Styles::simCompStyles.paddingX * 2.f);
        size_t maxRows = std::max(m_inputSlots.size(), m_outputSlots.size());
        float height = ((float)maxRows * Styles::SIM_COMP_SLOT_ROW_SIZE);

        width = std::max(width, 100.f);
        height +=
            Styles::simCompStyles.headerHeight + Styles::simCompStyles.rowGap;

        width = glm::round(width / SNAP_AMOUNT) * SNAP_AMOUNT;
        height = glm::round(height / SNAP_AMOUNT) * SNAP_AMOUNT;

        return {width, height};
    }

    float SimulationSceneComponent::getSlotStartY() const {
        return Styles::SIM_COMP_SLOT_START_Y +
               (Styles::SIM_COMP_SLOT_ROW_SIZE / 2.f);
    }

    void SimulationSceneComponent::resetSlotPositions(const SceneState &state) {
        const auto [inpPositions, outPositions] =
            calculateSlotPositions(m_inputSlots.size(), m_outputSlots.size());

        for (size_t i = 0; i < inpPositions.size(); i++) {
            const auto slotComp =
                state.getComponentByUuid<SlotSceneComponent>(m_inputSlots[i]);
            slotComp->setPosition(inpPositions[i]);
        }

        for (size_t i = 0; i < outPositions.size(); i++) {
            const auto slotComp =
                state.getComponentByUuid<SlotSceneComponent>(m_outputSlots[i]);
            BESS_ASSERT(slotComp,
                        "Slot component with UUID {} not found in scene state",
                        (uint64_t)m_outputSlots[i]);
            slotComp->setPosition(outPositions[i]);
        }
    }

    void SimulationSceneComponent::resetSchematicPinsPositions(
        const SceneState &state) {
        // Schematic diagram pin positions
        // We will ignore resize slots for schematic view positioning
        // Resize slots will be hidden in schematic view.
        auto inpCount = m_inputSlots.size();
        auto outCount = m_outputSlots.size();
        if (inpCount != 0 &&
            state.getComponentByUuid<SlotSceneComponent>(m_inputSlots.back())
                ->isResizeSlot()) {
            inpCount -= 1;
        }

        if (outCount != 0 &&
            state.getComponentByUuid<SlotSceneComponent>(m_outputSlots.back())
                ->isResizeSlot()) {
            outCount -= 1;
        }

        const float inpOffsetY =
            (m_schematicTransform.scale.y / ((float)inpCount + 1.f));
        const float outOffsetY =
            (m_schematicTransform.scale.y / ((float)outCount + 1.f));
        const float startY = -(m_schematicTransform.scale.y / 2.f);

        float inpStartX = -m_schematicTransform.scale.x / 2.f;
        float outStartX = m_schematicTransform.scale.x / 2.f;

        if (m_style.schematicStyle.flipSlotsX) {
            auto temp = inpStartX;
            inpStartX = outStartX;
            outStartX = temp;
        }

        for (size_t i = 0; i < inpCount; i++) {
            const auto slotComp =
                state.getComponentByUuid<SlotSceneComponent>(m_inputSlots[i]);
            auto pos =
                glm::vec2(inpStartX, startY + (inpOffsetY * (float)(i + 1)));
            pos.y = glm::round(pos.y / SNAP_AMOUNT) * SNAP_AMOUNT;
            slotComp->setSchematicPos(glm::vec3(pos, -0.0005f));
        }

        for (size_t i = 0; i < outCount; i++) {
            const auto slotComp =
                state.getComponentByUuid<SlotSceneComponent>(m_outputSlots[i]);
            auto pos =
                glm::vec2(outStartX, startY + (outOffsetY * (float)(i + 1)));
            pos.y = glm::round(pos.y / SNAP_AMOUNT) * SNAP_AMOUNT;
            slotComp->setSchematicPos(glm::vec3(pos, -0.0005));
        }
    }

    size_t SimulationSceneComponent::getInputSlotsCount() const {
        return m_inputSlots.size();
    }

    size_t SimulationSceneComponent::getOutputSlotsCount() const {
        return m_outputSlots.size();
    }

    void SimulationSceneComponent::addOutputSlot(UUID slotId,
                                                 bool isLastResizeable) {
        if (isLastResizeable) {
            m_outputSlots.insert(m_outputSlots.end() - 1, slotId);
        } else {
            m_outputSlots.emplace_back(slotId);
        }
    }

    void SimulationSceneComponent::addInputSlot(UUID slotId,
                                                bool isLastResizeable) {
        if (isLastResizeable) {
            m_inputSlots.insert(m_inputSlots.end() - 1, slotId);
        } else {
            m_inputSlots.emplace_back(slotId);
        }
    }

    void SimulationSceneComponent::setScaleDirty(bool val) {
        m_isScaleDirty = val;
    }

    void SimulationSceneComponent::onAttach(SceneState &state) {
        SceneComponent::onAttach(state);
        BESS_ASSERT(m_compDef,
                    "SimSceneComp: Component definition must be set "
                    "before attaching to scene");

        if (m_simEngineId != UUID::null)
            return;

        auto &appCtx = Bess::GAppContext::getInstance();
        auto projectCtx = appCtx.getSubSystem<Bess::ProjectContext>();
        auto &simEngine = projectCtx->getSimEngine();
        m_simEngineId = simEngine.addComponent(m_compDef, false);
    }

    std::vector<UUID> SimulationSceneComponent::cleanup(SceneState &state,
                                                        UUID caller) {
        std::vector<UUID> removedIds;

        const auto ids = SceneComponent::cleanup(state, caller);
        removedIds.insert(removedIds.end(), ids.begin(), ids.end());

        auto &appCtx = Bess::GAppContext::getInstance();
        auto projectCtx = appCtx.getSubSystem<Bess::ProjectContext>();
        auto &simEngine = projectCtx->getSimEngine();
        simEngine.deleteComponent(m_simEngineId);
        m_simEngineId = UUID::null;

        return removedIds;
    }

    void
    SimulationSceneComponent::calculateSchematicScale(const SceneState &state) {
        auto inpCount = m_inputSlots.size();
        auto outCount = m_outputSlots.size();
        if (inpCount != 0 &&
            state.getComponentByUuid<SlotSceneComponent>(m_inputSlots.back())
                ->isResizeSlot()) {
            inpCount -= 1;
        }

        if (outCount != 0 &&
            state.getComponentByUuid<SlotSceneComponent>(m_outputSlots.back())
                ->isResizeSlot()) {
            outCount -= 1;
        }

        float maxInpSlotWidth = 0.f, maxOutSlotWidth = 0.f;
        for (size_t i = 0; i < inpCount; i++) {
            const auto slotComp =
                state.getComponentByUuid<SlotSceneComponent>(m_inputSlots[i]);
            const auto slotLabelSize =
                Core::Renderer::IRenderer2D::getTextRenderSize(
                    slotComp->getName(),
                    {.fontSize = Styles::componentStyles.slotLabelSize});
            maxInpSlotWidth = std::max(maxInpSlotWidth, slotLabelSize.x);
        }

        for (size_t i = 0; i < outCount; i++) {
            const auto slotComp =
                state.getComponentByUuid<SlotSceneComponent>(m_outputSlots[i]);
            const auto slotLabelSize =
                Core::Renderer::IRenderer2D::getTextRenderSize(
                    slotComp->getName(),
                    {.fontSize = Styles::componentStyles.slotLabelSize});
            maxOutSlotWidth = std::max(maxOutSlotWidth, slotLabelSize.x);
        }

        const size_t maxRows = std::max(inpCount, outCount);
        const float height =
            ((float)maxRows * Styles::SCHEMATIC_VIEW_PIN_ROW_SIZE);

        const auto textWidth =
            Core::Renderer::IRenderer2D::getTextRenderSize(
                m_name, {.fontSize = Styles::compSchematicStyles.nameFontSize})
                .x;

        float width = textWidth + (Styles::compSchematicStyles.paddingX *
                                   2.f); // keep the same width as normal view
        width += maxInpSlotWidth + maxOutSlotWidth;

        m_schematicTransform.scale = {width, height};
        m_schematicTransform.scale =
            glm::round(m_schematicTransform.scale / SNAP_AMOUNT) * SNAP_AMOUNT;
        m_isSchematicScaleDirty = false;
    }

    std::vector<std::shared_ptr<SceneComponent>>
    SimulationSceneComponent::createNew(
        const std::shared_ptr<SimEngine::Drivers::CompDef> &compDef) {

        const auto def =
            std::dynamic_pointer_cast<SimEngine::Drivers::Digital::DigCompDef>(
                compDef);

        const bool isInput =
            def->getBehaviorType() == SimEngine::ComponentBehaviorType::input;
        const bool isOutput =
            def->getBehaviorType() == SimEngine::ComponentBehaviorType::output;

        if (isInput) {
            return createNew<InputSceneComponent>(compDef);
        } else {
            return createNew<SimulationSceneComponent>(compDef);
        }
    }

    void SimulationSceneComponent::onMouseDragged(
        const Events::MouseDraggedEvent &e) {
        if (!m_isDragging) {
            onMouseDragBegin(e);
        }

        auto newPos = e.mousePos + m_dragOffset;
        newPos = glm::round(newPos / SNAP_AMOUNT) * SNAP_AMOUNT;

        const bool isSchematic =
            Bess::UI::UIMain::getTargetSceneViewportPanel()->isSchematicMode();

        if (isSchematic) {
            m_schematicTransform.position =
                glm::vec3(newPos, m_schematicTransform.position.z);
        } else {
            setPosition(glm::vec3(newPos, m_transform.position.z));
            if (m_isFirstSchematicDraw) {
                m_schematicTransform.position =
                    glm::vec3(newPos, m_schematicTransform.position.z);
            }
        }
    }

    glm::vec3
    SimulationSceneComponent::getAbsolutePosition(const SceneState &state,
                                                  bool isSchematicMode) const {
        if (isSchematicMode) {
            if (m_parentComponent == UUID::null) {
                return m_schematicTransform.position;
            }

            auto parentComp = state.getComponentByUuid(m_parentComponent);
            if (!parentComp) {
                return m_schematicTransform.position;
            }

            return parentComp->getAbsolutePosition(state, isSchematicMode) +
                   m_schematicTransform.position;
        } else {
            return SceneComponent::getAbsolutePosition(state, isSchematicMode);
        }
    }

    void SimulationSceneComponent::onTransformChanged() {
        m_schematicTransform.position.z = m_transform.position.z;
    }

    std::vector<UUID>
    SimulationSceneComponent::getDependants(const SceneState &state) const {
        std::vector<UUID> dependants;

        // get slots and their dependants
        for (const auto &inp : std::ranges::reverse_view(m_inputSlots)) {
            const auto &slotComp =
                state.getComponentByUuid<SlotSceneComponent>(inp);
            const auto &slotDeps = slotComp->getDependants(state);
            dependants.insert(
                dependants.end(), slotDeps.begin(), slotDeps.end());
            dependants.push_back(inp);
        }

        for (const auto &out : std::ranges::reverse_view(m_outputSlots)) {
            const auto &slotComp =
                state.getComponentByUuid<SlotSceneComponent>(out);
            const auto &slotDeps = slotComp->getDependants(state);
            dependants.insert(
                dependants.end(), slotDeps.begin(), slotDeps.end());
            dependants.push_back(out);
        }

        // get children and their dependants
        for (const auto &childId : m_childComponents) {
            if (std::ranges::find(dependants, childId) != dependants.end()) {
                continue;
            }
            const auto &childComp = state.getComponentByUuid(childId);
            const auto &childDeps = childComp->getDependants(state);
            dependants.insert(
                dependants.end(), childDeps.begin(), childDeps.end());
            dependants.push_back(childId);
        }

        return dependants;
    }

    void SimulationSceneComponent::setSchematicScaleDirty(bool val) {
        m_isSchematicScaleDirty = val;
    }

    void SimulationSceneComponent::onChildrenChanged() {
        setScaleDirty();
        setSchematicScaleDirty();
    }

    std::vector<std::shared_ptr<SlotSceneComponent>>
    SimulationSceneComponent::createIOSlots(size_t inputCount,
                                            size_t outputCount) {
        std::vector<std::shared_ptr<SlotSceneComponent>> slots;

        for (size_t i = 0; i < inputCount; i++) {
            auto slot = std::make_shared<SlotSceneComponent>();
            slot->setSlotType(SlotType::digitalInput);
            slot->setIndex(static_cast<int>(i));
            m_inputSlots.push_back(slot->getUuid());
            slots.push_back(slot);
        }

        for (size_t i = 0; i < outputCount; i++) {
            auto slot = std::make_shared<SlotSceneComponent>();
            slot->setSlotType(SlotType::digitalOutput);
            slot->setIndex(static_cast<int>(i));
            m_outputSlots.push_back(slot->getUuid());
            slots.push_back(slot);
        }

        return slots;
    }

    std::vector<SimEngine::LogicState>
    SimulationSceneComponent::getInputStates(const SceneState &state) const {
        std::vector<SimEngine::LogicState> states;
        for (const auto &inp : m_inputSlots) {
            const auto &slotComp =
                state.getComponentByUuid<SlotSceneComponent>(inp);
            if (slotComp->isResizeSlot()) {
                continue;
            }
            states.push_back(slotComp->getSlotState(state).getLogicState());
        }
        return states;
    }

    std::vector<SimEngine::LogicState>
    SimulationSceneComponent::getOutputStates(const SceneState &state) const {
        std::vector<SimEngine::LogicState> states;
        for (const auto &inp : m_outputSlots) {
            const auto &slotComp =
                state.getComponentByUuid<SlotSceneComponent>(inp);
            if (slotComp->isResizeSlot()) {
                continue;
            }
            states.push_back(slotComp->getSlotState(state).getLogicState());
        }
        return states;
    }

    std::vector<std::shared_ptr<SceneComponent>>
    SimulationSceneComponent::cloneSimulationComponent(
        const SceneState &sceneState,
        const std::shared_ptr<SimulationSceneComponent> &clonedComponent)
        const {
        BESS_ASSERT(clonedComponent,
                    "Cannot clone a null simulation component");

        prepareClone(*clonedComponent);
        clonedComponent->setSimEngineId(UUID::null);
        clonedComponent->setNetId(UUID::null);
        clonedComponent->setInputSlots({});
        clonedComponent->setOutputSlots({});
        clonedComponent->setCompDef(m_compDef ? m_compDef->clone() : nullptr);

        std::vector<std::shared_ptr<SceneComponent>> clonedComponents;
        clonedComponents.push_back(clonedComponent);

        std::unordered_set<UUID> clonedChildren;

        const auto cloneSlot = [&](const UUID &slotId, const bool isInputSlot) {
            const auto slotComponent =
                sceneState.getComponentByUuid<SlotSceneComponent>(slotId);
            BESS_ASSERT(slotComponent,
                        "Simulation child slot was not found during clone");

            const auto slotClones = slotComponent->clone(sceneState);
            BESS_ASSERT(slotClones.size() == 1,
                        "Slot clone should only produce one component");

            const auto clonedSlot =
                std::dynamic_pointer_cast<SlotSceneComponent>(
                    slotClones.front());
            BESS_ASSERT(clonedSlot,
                        "Cloned simulation child is not a slot component");

            if (isInputSlot) {
                clonedComponent->addInputSlot(clonedSlot->getUuid(), false);
            } else {
                clonedComponent->addOutputSlot(clonedSlot->getUuid(), false);
            }

            clonedComponents.insert(
                clonedComponents.end(), slotClones.begin(), slotClones.end());
            clonedChildren.insert(slotId);
        };

        for (const auto &childId : m_inputSlots) {
            const auto childComponent = sceneState.getComponentByUuid(childId);
            BESS_ASSERT(
                childComponent,
                "Simulation child component was not found during clone");
            auto slot = childComponent->cast<SlotSceneComponent>();
            cloneSlot(childId, true);
        }

        for (const auto &childId : m_outputSlots) {
            const auto childComponent = sceneState.getComponentByUuid(childId);
            BESS_ASSERT(
                childComponent,
                "Simulation child component was not found during clone");
            auto slot = childComponent->cast<SlotSceneComponent>();
            cloneSlot(childId, false);
        }

        for (const auto &childId : m_childComponents) {

            if (clonedChildren.contains(childId)) {
                continue;
            }

            const auto childComponent = sceneState.getComponentByUuid(childId);
            BESS_ASSERT(
                childComponent,
                "Simulation child component was not found during clone");

            const auto childClones = childComponent->clone(sceneState);
            BESS_ASSERT(!childClones.empty(),
                        "Simulation child clone returned no components");

            clonedComponents.insert(
                clonedComponents.end(), childClones.begin(), childClones.end());

            clonedChildren.insert(childId);
        }

        return clonedComponents;
    }

    void SimulationSceneComponent::drawPropertiesUI(SceneState &state) {
        SceneComponent::drawPropertiesUI(state);

        // Width and Height
        if (Widgets::TreeNode(0, "Size")) {
            float width = m_transform.scale.x;
            float height = m_transform.scale.y;
            if (ImGui::InputFloat("Width", &width, 1.f, 1000.f) ||
                ImGui::InputFloat("Height", &height, 1.f, 1000.f)) {
                setScale(glm::vec2(width, height));
                resetSlotPositions(state);
                setSchematicScaleDirty();
            }
            ImGui::TreePop();
        }

        // Input Slots Names
        if (Widgets::TreeNode(0, "Input Slots")) {
            for (size_t i = 0; i < m_inputSlots.size(); i++) {
                const auto slotComp =
                    state.getComponentByUuid<SlotSceneComponent>(
                        m_inputSlots[i]);
                if (slotComp->isResizeSlot()) {
                    continue;
                }
                std::string label = "Input Slot " + std::to_string(i);
                if (Widgets::TextBox(label, slotComp->getName())) {
                    slotComp->onNameChanged();
                }
            }
            ImGui::TreePop();
        }

        // Output Slots Names
        if (Widgets::TreeNode(0, "Output Slots")) {
            for (size_t i = 0; i < m_outputSlots.size(); i++) {
                const auto slotComp =
                    state.getComponentByUuid<SlotSceneComponent>(
                        m_outputSlots[i]);
                if (slotComp->isResizeSlot()) {
                    continue;
                }
                std::string label = "Output Slot " + std::to_string(i);
                if (Widgets::TextBox(label, slotComp->getName())) {
                    slotComp->onNameChanged();
                }
            }
            ImGui::TreePop();
        }
    }

    void SimulationSceneComponent::onNameChanged() {
        SceneComponent::onNameChanged();
        setScaleDirty();
        setSchematicScaleDirty();
    }

    void SimulationSceneComponent::setSchSlotsPosDirty(bool val) {
        m_isSchSlotsPosDirty = val;
    }
} // namespace Bess::Canvas
