#pragma once

#include "component.h"
#include "common/digital_state.h"
#include "json.hpp"

namespace Bess::Simulator::Components {
    class Slot : public Component {
    public:
        Slot(const uuids::uuid& uid, const uuids::uuid& parentUid, int renderId, ComponentType type);
        ~Slot() = default;

        void update(const glm::vec3& pos, const std::string& label);
        void update(const glm::vec3& pos, const glm::vec2& labelOffset);
        void update(const glm::vec3& pos, const glm::vec2& labelOffset, const std::string& label);
        void update(const glm::vec3& pos);

        void render() override;

        void deleteComponent() override;

        void addConnection(const uuids::uuid& uId, bool simulate = true);
        bool isConnectedTo(const uuids::uuid& uId);

        void highlightBorder(bool highlight = true);

        Simulator::DigitalState getState() const;
        DigitalState flipState();

        void setState(const uuids::uuid& uid, Simulator::DigitalState state, bool forceUpdate = false);

        const uuids::uuid& getParentId();

        void generate(const glm::vec3& pos = { 0.f, 0.f, 0.f }) override;

        nlohmann::json toJson();

        static uuids::uuid fromJson(const nlohmann::json& data, const uuids::uuid& parentuid);

        const std::string& getLabel();
        void setLabel(const std::string& label);

        const glm::vec2& getLabelOffset();
        void setLabelOffset(const glm::vec2& label);

        const std::vector<uuids::uuid>& getConnections();

        // uid of component making change
        void simulate(const uuids::uuid& uid, DigitalState state);

        void refresh(const uuids::uuid& uid, DigitalState state);

        void removeConnection(const uuids::uuid& uid);

    private:
        // contains ids of slots
        std::vector<uuids::uuid> m_connections;
        bool m_highlightBorder = false;
        void onLeftClick(const glm::vec2& pos);
        void onMouseHover();

        // slot specific
        const uuids::uuid m_parentUid;
        Simulator::DigitalState m_state;

        void onChange();

        std::unordered_map<uuids::uuid, bool> m_stateChangeHistory = {};

        std::string m_label = "";
        glm::vec2 m_labelOffset = { 0.f, 0.f };
        float m_labelWidth = 0.f;

        float m_deleting = false;

        void calculateLabelWidth(float fontSize);
    };
} // namespace Bess::Simulator::Components
