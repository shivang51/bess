#include "components_manager/jcomponent_data.h"

namespace Bess::Simulator::Components {
	JComponentData::JComponentData(const nlohmann::json& data, const std::string& collectionName) {
		m_name = data["name"];
		m_collectionName = collectionName;
		m_inputCount = data["inputCount"];
		for (auto& expr : data["outputs"]) {
			m_outputs.emplace_back(expr);
		}
	}

    const std::string& JComponentData::getName() const {
        return m_name;
    }

    const std::string& JComponentData::getCollectionName() const {
        return m_collectionName;
    }

    const int& JComponentData::getInputCount() const {
        return m_inputCount;
    }

    const std::vector<std::string>& JComponentData::getOutputs() const {
        return m_outputs;
    }
}