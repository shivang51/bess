#include "net/net.h"
#include <algorithm>

namespace Bess::SimEngine {

    void Net::addComponents(const std::vector<UUID> &componentUuids) {
        m_components.insert(m_components.end(), componentUuids.begin(), componentUuids.end());
    }

    void Net::join(Net &other) {
        for (const auto &comp_uuid : other.m_components) {
            m_components.emplace_back(comp_uuid);
        }
        other.clear();
    }
    void Net::removeComponent(const UUID &componentUuid) {
        m_components.erase(std::ranges::remove(m_components, componentUuid).begin(), m_components.end());
    }

    void Net::removeComponents(const std::vector<UUID> &componentUuids) {
        for (const auto &componentUuid : componentUuids) {
            m_components.erase(std::ranges::remove(m_components, componentUuid).begin(), m_components.end());
        }
    }

    const std::vector<UUID> &Net::getComponents() const {
        return m_components;
    }

    void Net::clear() {
        m_components.clear();
    }

    void Net::addComponent(const UUID &component_uuid) {

        m_components.emplace_back(component_uuid);
    }

    void Net::setUUID(const UUID &uuid) {
        m_uuid = uuid;
    }

    const UUID &Net::getUUID() const {
        return m_uuid;
    }

    size_t Net::size() const {
        return m_components.size();
    }

    void Net::setComponents(const std::vector<UUID> &componentUuids) {
        m_components = componentUuids;
    }
} // namespace Bess::SimEngine

namespace Bess::JsonConvert {
    void toJsonValue(const Bess::SimEngine::Net &net, Json::Value &j) {
        j = Json::objectValue;
        toJsonValue(net.getUUID(), j["uuid"]);
        j["components"] = Json::arrayValue;
        for (const auto &compUuid : net.getComponents()) {
            Json::Value compJ;
            toJsonValue(compUuid, compJ);
            j["components"].append(compJ);
        }
    }

    void fromJsonValue(const Json::Value &j, Bess::SimEngine::Net &net) {
        if (j.isObject()) {
            if (j.isMember("uuid")) {
                UUID uuid;
                fromJsonValue(j["uuid"], uuid);
                net.setUUID(uuid);
            }
            if (j.isMember("components") && j["components"].isArray()) {
                std::vector<UUID> components;
                for (const auto &compJ : j["components"]) {
                    UUID compUuid;
                    fromJsonValue(compJ, compUuid);
                    components.emplace_back(compUuid);
                }
                net.setComponents(components);
            }
        }
    }
} // namespace Bess::JsonConvert
