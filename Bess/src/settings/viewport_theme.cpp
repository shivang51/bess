#include "settings/viewport_theme.h"
#include "imgui.h"

namespace Bess {
    glm::vec4 ViewportTheme::backgroundColor;
    glm::vec4 ViewportTheme::componentBGColor;
    glm::vec4 ViewportTheme::componentBorderColor;
    glm::vec4 ViewportTheme::stateHighColor;
    glm::vec4 ViewportTheme::stateLowColor;
    glm::vec4 ViewportTheme::wireColor;
    glm::vec4 ViewportTheme::selectedWireColor;
    glm::vec4 ViewportTheme::compHeaderColor;
    glm::vec4 ViewportTheme::selectedCompColor;
    glm::vec4 ViewportTheme::textColor;
    glm::vec4 ViewportTheme::gridColor;
    glm::vec4 ViewportTheme::selectionBoxBorderColor;
    glm::vec4 ViewportTheme::selectionBoxFillColor;

    std::unordered_map<Bess::SimEngine::ComponentType, glm::vec4> ViewportTheme::s_compColorMap;

    void ViewportTheme::updateColorsFromImGuiStyle() {
        ImGuiStyle &style = ImGui::GetStyle();
        ImVec4 *colors = style.Colors;

        ImVec4 windowBg = colors[ImGuiCol_WindowBg];
        backgroundColor = glm::vec4(windowBg.x, windowBg.y, windowBg.z, windowBg.w);

        auto color = colors[ImGuiCol_WindowBg];
        componentBGColor = glm::vec4(color.x + 0.05f, color.y + 0.05f, color.z + 0.05f, 0.8f);
        componentBGColor = glm::clamp(componentBGColor, 0.0f, 1.0f); // Ensure values stay in [0, 1]

        auto color_ = 1.f - componentBGColor;
        float darkenFactor = 0.5f;
        componentBorderColor = glm::vec4(color_.x * darkenFactor, color_.y * darkenFactor, color_.z * darkenFactor, 0.8f);

        compHeaderColor = glm::vec4(0.8f, 0.1f, 0.1f, 0.8f);

        color = colors[ImGuiCol_Text];
        darkenFactor = 1.f;
        textColor = glm::vec4(color.x * darkenFactor, color.y * darkenFactor, color.z * darkenFactor, color.w);

        gridColor = textColor * 0.45f;

        stateHighColor = glm::vec4(0.6f, 0.8f, 0.4f, 1.00f);         // Greenish
        stateLowColor = glm::vec4(0.2f, 0.3f, 0.1f, 1.00f);          // Dark Greenish
        selectedWireColor = glm::vec4(1.0f, 0.64f, 0.0f, 1.0f);      // Orange
        selectionBoxBorderColor = glm::vec4(0.3f, 0.3f, 0.8f, 1.0f); // Blueish (corrected from 8.0)
        selectionBoxFillColor = glm::vec4(0.3f, 0.3f, 0.7f, 0.5f);   // Semi-transparent blue
        selectedCompColor = selectedWireColor;                       // Orange
        wireColor = componentBorderColor;

        initCompColorMap();
    }

    glm::vec4 ViewportTheme::getCompHeaderColor(Bess::SimEngine::ComponentType type) {
        if (!s_compColorMap.contains(type))
            return ViewportTheme::compHeaderColor;
        return s_compColorMap.at(type);
    }

    void ViewportTheme::initCompColorMap() {
        const glm::vec4 logicColor = glm::vec4(0.25f, 0.45f, 0.65f, 0.85f);         // Slate Blue
        const glm::vec4 memoryColor = glm::vec4(0.5f, 0.4f, 0.7f, 0.85f);           // Muted Purple
        const glm::vec4 arithmeticColor = glm::vec4(0.2f, 0.65f, 0.6f, 0.85f);      // Brighter Teal
        const glm::vec4 routingColor = glm::vec4(0.15f, 0.55f, 0.5f, 0.85f);        // Base Teal
        const glm::vec4 encoderDecoderColor = glm::vec4(0.1f, 0.45f, 0.45f, 0.85f); // Darker Teal
        const glm::vec4 comparatorColor = glm::vec4(0.3f, 0.6f, 0.55f, 0.85f);      // Lighter Teal
        const glm::vec4 specialColor = glm::vec4(0.4f, 0.4f, 0.4f, 0.8f);           // Neutral Grey

        s_compColorMap.clear();

        // --- Special / Uncategorized ---
        s_compColorMap[Bess::SimEngine::ComponentType::EMPTY] = specialColor;

        // --- Category: Logic & I/O ---
        s_compColorMap[Bess::SimEngine::ComponentType::INPUT] = logicColor;
        s_compColorMap[Bess::SimEngine::ComponentType::OUTPUT] = logicColor;
        s_compColorMap[Bess::SimEngine::ComponentType::AND] = logicColor;
        s_compColorMap[Bess::SimEngine::ComponentType::OR] = logicColor;
        s_compColorMap[Bess::SimEngine::ComponentType::NOT] = logicColor;
        s_compColorMap[Bess::SimEngine::ComponentType::NOR] = logicColor;
        s_compColorMap[Bess::SimEngine::ComponentType::NAND] = logicColor;
        s_compColorMap[Bess::SimEngine::ComponentType::XOR] = logicColor;
        s_compColorMap[Bess::SimEngine::ComponentType::XNOR] = logicColor;

        // --- Category: Sequential / Flip-Flops ---
        s_compColorMap[Bess::SimEngine::ComponentType::FLIP_FLOP_JK] = memoryColor;
        s_compColorMap[Bess::SimEngine::ComponentType::FLIP_FLOP_SR] = memoryColor;
        s_compColorMap[Bess::SimEngine::ComponentType::FLIP_FLOP_D] = memoryColor;
        s_compColorMap[Bess::SimEngine::ComponentType::FLIP_FLOP_T] = memoryColor;

        // --- Sub-Category: Arithmetic ---
        s_compColorMap[Bess::SimEngine::ComponentType::FULL_ADDER] = arithmeticColor;
        s_compColorMap[Bess::SimEngine::ComponentType::HALF_ADDER] = arithmeticColor;
        s_compColorMap[Bess::SimEngine::ComponentType::FULL_SUBTRACTOR] = arithmeticColor;
        s_compColorMap[Bess::SimEngine::ComponentType::HALF_SUBTRACTOR] = arithmeticColor;

        // --- Sub-Category: Data Routing ---
        s_compColorMap[Bess::SimEngine::ComponentType::MULTIPLEXER_4_1] = routingColor;
        s_compColorMap[Bess::SimEngine::ComponentType::MULTIPLEXER_2_1] = routingColor;
        s_compColorMap[Bess::SimEngine::ComponentType::DEMUX_1_4] = routingColor;

        // --- Sub-Category: Encoders/Decoders ---
        s_compColorMap[Bess::SimEngine::ComponentType::DECODER_2_4] = encoderDecoderColor;
        s_compColorMap[Bess::SimEngine::ComponentType::ENCODER_4_2] = encoderDecoderColor;
        s_compColorMap[Bess::SimEngine::ComponentType::PRIORITY_ENCODER_4_2] = encoderDecoderColor;

        // --- Sub-Category: Comparators ---
        s_compColorMap[Bess::SimEngine::ComponentType::COMPARATOR_1_BIT] = comparatorColor;
        s_compColorMap[Bess::SimEngine::ComponentType::COMPARATOR_2_BIT] = comparatorColor;
    }
} // namespace Bess
