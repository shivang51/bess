#pragma once

#include <string>
#include <vector>
#include "json.hpp"

namespace Bess::Simulator::Components {
	class JComponentData {
	public:
		JComponentData() = default;
		JComponentData(const nlohmann::json& json, const std::string& collectionName);

		const std::string& getName() const;

		const std::string& getCollectionName() const;

		const int& getInputCount() const;

		const std::vector<std::string>& getOutputs() const;

	private:
		std::string m_name = "";
		std::string m_collectionName = "";
		int m_inputCount = 0;
		std::vector<std::string> m_outputs = {};
	};
}