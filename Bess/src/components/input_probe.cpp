#include "components/input_probe.h"
#include "common/bind_helpers.h"
#include "common/helpers.h"
#include "pages/main_page/main_page_state.h"
#include "scene/renderer/renderer.h"
#include "settings/viewport_theme.h"

namespace Bess::Simulator::Components {
    InputProbe::InputProbe() : Component() {
    }

    InputProbe::InputProbe(const uuids::uuid &uid, int renderId,
                           glm::vec3 position, const uuids::uuid &outputSlot)
        : Component(uid, renderId, position, ComponentType::inputProbe) {
        m_name = "Input Probe";
        m_outputSlot = outputSlot;
        m_events[ComponentEventType::leftClick] = (OnLeftClickCB)BIND_FN_1(InputProbe::onLeftClick);
        m_transform.setScale({65.f, 25.f});
    }

    void InputProbe::render() {

        float thickness = 1.f;

        Slot *slot =
            (Slot *)Simulator::ComponentsManager::components[m_outputSlot].get();

        float r = 16.f;

        auto bgColor = ViewportTheme::componentBGColor;
        auto borderColor = ViewportTheme::componentBorderColor;
        auto isHigh = slot->getState() == DigitalState::high;

        std::string label = (isHigh) ? "On" : "Off";

        if (m_isSelected) {
            borderColor = ViewportTheme::selectedCompColor;
        }

        auto pos = m_transform.getPosition();
        auto size = m_transform.getScale();

        Renderer2D::Renderer::quad(pos, size, bgColor, m_renderId, glm::vec4(r), true, borderColor, thickness);

        slot->update(pos + glm::vec3({(size.x / 2) - 12.f, 0.f, 0.f}), {-12.f, 0.f}, label);
        slot->render();
    }

    void InputProbe::deleteComponent() {
        ComponentsManager::deleteComponent(m_outputSlot);
    }

    void InputProbe::generate(const glm::vec3 &pos) {
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
        ComponentsManager::components[uid] = std::make_shared<Components::InputProbe>(uid, renderId, pos_, slotId);
        ComponentsManager::addRenderIdToCId(renderId, uid);
        ComponentsManager::addCompIdToRId(renderId, uid);
        ComponentsManager::renderComponents.emplace_back(uid);
    }

    nlohmann::json InputProbe::toJson() {
        nlohmann::json data;
        data["uid"] = Common::Helpers::uuidToStr(m_uid);
        data["type"] = (int)m_type;
        data["pos"] = Common::Helpers::EncodeVec3(m_transform.getPosition());
        auto slot = (Slot *)ComponentsManager::components[m_outputSlot].get();
        data["slot"] = slot->toJson();
        return data;
    }

    void InputProbe::fromJson(const nlohmann::json &data) {
        uuids::uuid uid = Common::Helpers::strToUUID(static_cast<std::string>(data["uid"]));

        uuids::uuid slotId = Slot::fromJson(data["slot"], uid);

        auto pos = Common::Helpers::DecodeVec3(data["pos"]);
        pos.z = ComponentsManager::getNextZPos();

        auto renderId = ComponentsManager::getNextRenderId();

        ComponentsManager::components[uid] = std::make_shared<Components::InputProbe>(uid, renderId, pos, slotId);
        ComponentsManager::addRenderIdToCId(renderId, uid);
        ComponentsManager::addCompIdToRId(renderId, uid);
        ComponentsManager::renderComponents.emplace_back(uid);
    }

    void InputProbe::simulate() const {
        auto slot = (Slot *)ComponentsManager::components[m_outputSlot].get();
        slot->simulate(m_uid, slot->getState());
    }

    void InputProbe::refresh() const {
        auto slot = (Slot *)ComponentsManager::components[m_outputSlot].get();
        slot->refresh(m_uid, slot->getState());
    }

    void InputProbe::onLeftClick(const glm::vec2 &pos) {
        Pages::MainPageState::getInstance()->setBulkId(m_uid);
        auto slot = (Slot *)ComponentsManager::components[m_outputSlot].get();
        slot->flipState();
    }

} // namespace Bess::Simulator::Components
