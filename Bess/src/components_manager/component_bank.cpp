#include "components_manager/component_bank.h"

#include <fstream>

namespace Bess::Simulator{
    ComponentBank::BankVault ComponentBank::m_vault{};

    void Simulator::ComponentBank::addToCollection(std::string collectionName, ComponentBankElement element)
    {
        m_vault[collectionName].emplace_back(element);
    }

    const ComponentBank::BankVault& Simulator::ComponentBank::getVault()
    {
        return m_vault;
    }

    void Simulator::ComponentBank::loadFromJson(const std::string& filepath)
    {
        std::ifstream file(filepath);
        nlohmann::json data = nlohmann::json::parse(file);
        std::string collectionName = data["collectionName"];
        for (auto& cjson : data["components"]) {
            Components::JComponentData compData(cjson,collectionName);
            ComponentBankElement el(ComponentType::jcomponent, compData.getName(), compData);
            m_vault[collectionName].emplace_back(el);
        }
    }
    
    Simulator::ComponentBankElement::ComponentBankElement(ComponentType type, const std::string& name): m_type(type), m_name(name)
    {

    }
    
    Simulator::ComponentBankElement::ComponentBankElement(ComponentType type, const std::string& name, const Components::JComponentData& jcompData): m_type(type), m_name(name)
    {
        m_jcompData = std::make_shared<Components::JComponentData>(jcompData);
    }

    const std::string& Simulator::ComponentBankElement::getName() const
    {
        return m_name;
    }
    
    const ComponentType& Simulator::ComponentBankElement::getType() const
    {
        return m_type;
    }
    
    const std::shared_ptr<Components::JComponentData> Simulator::ComponentBankElement::getJCompData() const
    {
        return m_jcompData;
    }
}