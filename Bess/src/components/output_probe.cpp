#include "components/output_probe.h"

#include "common/helpers.h"
#include "components/connection.h"
#include "pages/main_page/main_page_state.h"
#include "scene/renderer/renderer.h"
#include "settings/viewport_theme.h"

namespace Bess::Simulator::Components {

    OutputProbe::OutputProbe() : Component() {
    }

    OutputProbe::OutputProbe(const uuids::uuid &uid, int renderId,
                             glm::vec3 position, const uuids::uuid &outputSlot)
        : Component(uid, renderId, position, ComponentType::outputProbe) {
        m_name = "Output Probe";
        m_inputSlot = outputSlot;
        m_events[ComponentEventType::leftClick] =
            (OnLeftClickCB)[&](const glm::vec2 &pos) {
            Pages::MainPageState::getInstance()->setBulkId(m_uid);
        };
        m_transform.setScale({50.f, 25.f});
    }

    void OutputProbe::render() {
        float thickness = 1.f;

        Slot *slot = (Slot *)Simulator::ComponentsManager::components[m_inputSlot].get();

        float r = 12.f;

        auto bgColor = ViewportTheme::componentBGColor;
        auto borderColor = ViewportTheme::componentBorderColor;

        std::string label = "L";
        if (slot->getState() == DigitalState::high) {
            borderColor = ViewportTheme::stateHighColor;
            label = "H";
        }

        auto pos = m_transform.getPosition();
        auto size = m_transform.getScale();

        Renderer2D::Renderer::quad(
            pos,
            size,
            bgColor,
            m_renderId,
            glm::vec4(r),
            true,
            borderColor,
            thickness);

        slot->update(pos + glm::vec3({-(size.x / 2) + 10.f, 0.f, 0.f}), {12.f, 0.f}, label);
        slot->render();
    }

    void OutputProbe::deleteComponent() {
        ComponentsManager::deleteComponent(m_inputSlot);
    }

    void OutputProbe::generate(const glm::vec3 &pos) {
        auto uid = Common::Helpers::uuidGenerator.getUUID();

        auto slotId = Common::Helpers::uuidGenerator.getUUID();
        auto renderId = ComponentsManager::getNextRenderId();

        ComponentsManager::components[slotId] = std::make_shared<Components::Slot>(
            slotId, uid, renderId, ComponentType::inputSlot);

        ComponentsManager::addRenderIdToCId(renderId, slotId);
        ComponentsManager::addCompIdToRId(renderId, slotId);

        auto pos_ = pos;
        pos_.z = ComponentsManager::getNextZPos();

        renderId = ComponentsManager::getNextRenderId();
        ComponentsManager::components[uid] =
            std::make_shared<Components::OutputProbe>(uid, renderId, pos_, slotId);
        ComponentsManager::addRenderIdToCId(renderId, uid);
        ComponentsManager::addCompIdToRId(renderId, uid);
        ComponentsManager::renderComponents.emplace_back(uid);
    }

    nlohmann::json OutputProbe::toJson() {
        nlohmann::json data;
        data["uid"] = Common::Helpers::uuidToStr(m_uid);
        data["type"] = (int)m_type;
        data["pos"] = Common::Helpers::EncodeVec3(m_transform.getPosition());
        auto slot = (Slot *)ComponentsManager::components[m_inputSlot].get();
        data["slot"] = slot->toJson();
        return data;
    }

    void OutputProbe::fromJson(const nlohmann::json &data) {
        uuids::uuid uid;
        uid = Common::Helpers::strToUUID(static_cast<std::string>(data["uid"]));
        auto pos = Common::Helpers::DecodeVec3(data["pos"]);
        pos.z = ComponentsManager::getNextZPos();

        uuids::uuid slotId = Slot::fromJson(data["slot"], uid);

        auto renderId = ComponentsManager::getNextRenderId();

        ComponentsManager::components[uid] = std::make_shared<Components::OutputProbe>(uid, renderId, pos, slotId);
        ComponentsManager::addRenderIdToCId(renderId, uid);
        ComponentsManager::addCompIdToRId(renderId, uid);
        ComponentsManager::renderComponents.emplace_back(uid);
    }
} // namespace Bess::Simulator::Components
