#include "settings/viewport_theme.h"
#include "imgui.h"

namespace Bess {
    SceneColors ViewportTheme::colors;

    std::unordered_map<Bess::SimEngine::ComponentType, glm::vec4> ViewportTheme::s_compHeaderColorMap;

    void ViewportTheme::updateColorsFromImGuiStyle() {
        ImGuiStyle &style = ImGui::GetStyle();
        ImVec4 *imguiColors = style.Colors;

        ImVec4 windowBg = imguiColors[ImGuiCol_WindowBg];
        colors.background = glm::vec4(windowBg.x, windowBg.y, windowBg.z, windowBg.w);

        auto compBg = imguiColors[ImGuiCol_WindowBg];
        colors.componentBG = glm::vec4(
            compBg.x + 0.05f,
            compBg.y + 0.05f,
            compBg.z + 0.05f,
            0.8f);
        colors.componentBG = glm::clamp(colors.componentBG, 0.0f, 1.0f);

        auto color_ = 1.f - colors.componentBG;
        float darkenFactor = 0.5f;
        colors.componentBorder = glm::vec4(
            color_.x * darkenFactor,
            color_.y * darkenFactor,
            color_.z * darkenFactor,
            0.8f);

        colors.componentBorder = glm::vec4(0.05f, 0.05f, 0.05f, 0.8f);

        colors.compHeader = glm::vec4(0.8f, 0.1f, 0.1f, 0.8f);

        auto textCol = imguiColors[ImGuiCol_Text];
        colors.text = glm::vec4(
            textCol.x,
            textCol.y,
            textCol.z,
            textCol.w);

        colors.grid = colors.text * 0.45f;

        colors.stateHigh = glm::vec4(0.6f, 0.8f, 0.4f, 1.00f); // Greenish
        colors.stateLow = glm::vec4(0.2f, 0.3f, 0.1f, 1.00f);  // Dark greenish

        colors.wire = colors.componentBorder;
        colors.ghostWire = glm::vec4(0.5f, 0.5f, 0.5f, 0.5f);
        colors.selectedWire = glm::vec4(1.0f, 0.64f, 0.0f, 1.0f); // Orange
        colors.clockConnectionHigh = glm::vec4(0.30f, 0.75f, 0.95f, 1.0f);
        colors.clockConnectionLow = glm::vec4(0.10f, 0.35f, 0.55f, 1.0f);

        colors.selectionBoxBorder = glm::vec4(0.3f, 0.3f, 0.8f, 1.0f); // Bluish
        colors.selectionBoxFill = glm::vec4(0.3f, 0.3f, 0.7f, 0.5f);   // Semi-transparent blue

        colors.selectedComp = colors.selectedWire; // Orange

        initCompColorMap();
    }

    glm::vec4 ViewportTheme::getCompHeaderColor(Bess::SimEngine::ComponentType type) {
        if (!s_compHeaderColorMap.contains(type))
            return colors.compHeader;
        return s_compHeaderColorMap.at(type);
    }

    void ViewportTheme::initCompColorMap() {
        const glm::vec4 logicColor = glm::vec4(0.25f, 0.45f, 0.65f, 0.85f);         // Slate Blue
        const glm::vec4 memoryColor = glm::vec4(0.5f, 0.4f, 0.7f, 0.85f);           // Muted Purple
        const glm::vec4 arithmeticColor = glm::vec4(0.2f, 0.65f, 0.6f, 0.85f);      // Brighter Teal
        const glm::vec4 routingColor = glm::vec4(0.15f, 0.55f, 0.5f, 0.85f);        // Base Teal
        const glm::vec4 encoderDecoderColor = glm::vec4(0.1f, 0.45f, 0.45f, 0.85f); // Darker Teal
        const glm::vec4 comparatorColor = glm::vec4(0.3f, 0.6f, 0.55f, 0.85f);      // Lighter Teal
        const glm::vec4 specialColor = glm::vec4(0.4f, 0.4f, 0.4f, 0.8f);           // Neutral Grey

        s_compHeaderColorMap.clear();

        s_compHeaderColorMap[Bess::SimEngine::ComponentType::EMPTY] = specialColor;

        s_compHeaderColorMap[Bess::SimEngine::ComponentType::INPUT] = logicColor;
        s_compHeaderColorMap[Bess::SimEngine::ComponentType::OUTPUT] = logicColor;
        s_compHeaderColorMap[Bess::SimEngine::ComponentType::AND] = logicColor;
        s_compHeaderColorMap[Bess::SimEngine::ComponentType::OR] = logicColor;
        s_compHeaderColorMap[Bess::SimEngine::ComponentType::NOT] = logicColor;
        s_compHeaderColorMap[Bess::SimEngine::ComponentType::NOR] = logicColor;
        s_compHeaderColorMap[Bess::SimEngine::ComponentType::NAND] = logicColor;
        s_compHeaderColorMap[Bess::SimEngine::ComponentType::XOR] = logicColor;
        s_compHeaderColorMap[Bess::SimEngine::ComponentType::XNOR] = logicColor;

        s_compHeaderColorMap[Bess::SimEngine::ComponentType::FLIP_FLOP_JK] = memoryColor;
        s_compHeaderColorMap[Bess::SimEngine::ComponentType::FLIP_FLOP_SR] = memoryColor;
        s_compHeaderColorMap[Bess::SimEngine::ComponentType::FLIP_FLOP_D] = memoryColor;
        s_compHeaderColorMap[Bess::SimEngine::ComponentType::FLIP_FLOP_T] = memoryColor;

        s_compHeaderColorMap[Bess::SimEngine::ComponentType::FULL_ADDER] = arithmeticColor;
        s_compHeaderColorMap[Bess::SimEngine::ComponentType::HALF_ADDER] = arithmeticColor;
        s_compHeaderColorMap[Bess::SimEngine::ComponentType::FULL_SUBTRACTOR] = arithmeticColor;
        s_compHeaderColorMap[Bess::SimEngine::ComponentType::HALF_SUBTRACTOR] = arithmeticColor;

        s_compHeaderColorMap[Bess::SimEngine::ComponentType::MULTIPLEXER_4_1] = routingColor;
        s_compHeaderColorMap[Bess::SimEngine::ComponentType::MULTIPLEXER_2_1] = routingColor;
        s_compHeaderColorMap[Bess::SimEngine::ComponentType::DEMUX_1_4] = routingColor;

        s_compHeaderColorMap[Bess::SimEngine::ComponentType::DECODER_2_4] = encoderDecoderColor;
        s_compHeaderColorMap[Bess::SimEngine::ComponentType::ENCODER_4_2] = encoderDecoderColor;
        s_compHeaderColorMap[Bess::SimEngine::ComponentType::PRIORITY_ENCODER_4_2] = encoderDecoderColor;

        s_compHeaderColorMap[Bess::SimEngine::ComponentType::COMPARATOR_1_BIT] = comparatorColor;
        s_compHeaderColorMap[Bess::SimEngine::ComponentType::COMPARATOR_2_BIT] = comparatorColor;
    }
} // namespace Bess
