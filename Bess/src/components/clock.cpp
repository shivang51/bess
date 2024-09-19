#include "components/clock.h"
#include "common/bind_helpers.h"
#include "common/helpers.h"
#include "components/slot.h"
#include "components_manager/components_manager.h"
#include "imgui.h"
#include "pages/main_page/main_page_state.h"
#include "renderer/renderer.h"
#include "settings/viewport_theme.h"
#include "ui/m_widgets.h"
#include <GLFW/glfw3.h>

namespace Bess::Simulator::Components {

    glm::vec2 clockSize = {65.f, 25.f};

    Clock::Clock(const uuids::uuid &uid, int renderId, glm::vec3 position, const uuids::uuid &slotUid) : Component(uid, renderId, position, ComponentType::clock) {
        m_frequency = 1.f;
        m_outputSlotId = slotUid;
        m_events[ComponentEventType::leftClick] = (OnLeftClickCB)BIND_FN_1(Clock::onLeftClick);
        m_name = "Clock";
    }

    void Clock::update() {
        Component::update();
        float frequency = m_frequency;
        switch (m_frequencyUnit) {
        case Bess::Simulator::FrequencyUnit::kiloHertz:
            frequency *= 1e3;
            break;
        case Bess::Simulator::FrequencyUnit::megaHertz:
            frequency *= 1e6;
            break;
        default:
            break;
        }

        float cycleTime = 1.f / frequency;
        double currTime = glfwGetTime();
        double delta = currTime - m_prevUpdateTime;

        if (delta > cycleTime) {
            m_prevUpdateTime = currTime;

            auto &comp = ComponentsManager::components[m_outputSlotId];
            auto slot = std::dynamic_pointer_cast<Components::Slot>(comp);
            slot->flipState();
        }
    }

    void Clock::drawProperties() {
        ImGui::DragFloat("Frequency", &m_frequency, 0.1f, 0.1f, 3.f);
        std::vector<std::string> frequencies = {"Hz", "kHz", "MHz"};
        std::string currFreq = frequencies[(int)m_frequencyUnit];
        if (UI::MWidgets::ComboBox("Unit", currFreq, frequencies)) {
            auto idx = std::distance(frequencies.begin(), std::find(frequencies.begin(), frequencies.end(), currFreq));
            m_frequencyUnit = static_cast<FrequencyUnit>(idx);
        }
    }

    void Clock::fromJson(const nlohmann::json &data) {
        uuids::uuid uid = Common::Helpers::strToUUID(static_cast<std::string>(data["uid"]));

        uuids::uuid slotId = Slot::fromJson(data["slot"], uid);

        auto pos = Common::Helpers::DecodeVec3(data["pos"]);
        pos.z = ComponentsManager::getNextZPos();

        float frequency = data["frequency"];

        auto renderId = ComponentsManager::getNextRenderId();

        ComponentsManager::components[uid] = std::make_shared<Components::Clock>(uid, renderId, pos, slotId);
        ComponentsManager::addRenderIdToCId(renderId, uid);
        ComponentsManager::addCompIdToRId(renderId, uid);
        ComponentsManager::renderComponents.emplace_back(uid);

        auto clockCmp = std::dynamic_pointer_cast<Clock>(ComponentsManager::components[uid]);
        clockCmp->setFrequency(frequency);
    }

    nlohmann::json Clock::toJson() {
        nlohmann::json data;
        data["uid"] = Common::Helpers::uuidToStr(m_uid);
        data["type"] = (int)m_type;
        data["pos"] = Common::Helpers::EncodeVec3(m_position);
        auto slot = (Slot *)ComponentsManager::components[m_outputSlotId].get();
        data["slot"] = slot->toJson();
        data["frequency"] = m_frequency;
        return data;
    }

    void Clock::setFrequency(float frequency) {
        m_frequency = frequency;
    }

    float Clock::getFrequency() {
        return m_frequency;
    }

    void Clock::render() {
        float thickness = 1.f;

        Slot *slot = (Slot *)Simulator::ComponentsManager::components[m_outputSlotId].get();

        float r = 16.f;

        auto bgColor = ViewportTheme::componentBGColor;
        auto borderColor = ViewportTheme::componentBorderColor;

        std::string label = "Clock";

        if (slot->getState() == DigitalState::high) {
            borderColor = ViewportTheme::stateHighColor;
        }

        Renderer2D::Renderer::quad(m_position, clockSize, bgColor, m_renderId, glm::vec4(r), borderColor, thickness);

        slot->update(m_position + glm::vec3({(clockSize.x / 2) - 12.f, 0.f, 0.f}), {-12.f, 0.f}, label);
        slot->render();
    }

    void Clock::generate(const glm::vec3 &pos) {
        auto uid = Common::Helpers::uuidGenerator.getUUID();

        auto slotId = Common::Helpers::uuidGenerator.getUUID();
        auto renderId = ComponentsManager::getNextRenderId();
        ComponentsManager::components[slotId] = std::make_shared<Components::Slot>(
            slotId, uid, renderId, ComponentType::outputSlot);
        ComponentsManager::addRenderIdToCId(renderId, slotId);
        ComponentsManager::addCompIdToRId(renderId, slotId);

        auto pos_ = pos;
        pos_.z = ComponentsManager::getNextZPos();

        renderId = ComponentsManager::getNextRenderId();
        ComponentsManager::components[uid] = std::make_shared<Components::Clock>(uid, renderId, pos_, slotId);
        ComponentsManager::addRenderIdToCId(renderId, uid);
        ComponentsManager::addCompIdToRId(renderId, uid);
        ComponentsManager::renderComponents.emplace_back(uid);
    }

    void Clock::deleteComponent() {
        ComponentsManager::deleteComponent(m_outputSlotId);
    }
    void Clock::onLeftClick(const glm::vec2 &pos) {
        auto mainPageState = Pages::MainPageState::getInstance();
        mainPageState->setBulkId(m_uid);
    }
} // namespace Bess::Simulator::Components
