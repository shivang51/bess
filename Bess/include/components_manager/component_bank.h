#pragma once

#include <unordered_map>
#include <string>
#include <vector>
#include <memory>

#include "component_type.h"
#include "jcomponent_data.h"

namespace Bess::Simulator {

    class ComponentBankElement {
    public:
        ComponentBankElement(ComponentType type, const std::string& name);
        ComponentBankElement(ComponentType type, const std::string& name, const Components::JComponentData& jcompData);
    
        const std::string& getName() const;
        const ComponentType& getType() const;
        const std::shared_ptr<Components::JComponentData> getJCompData() const;
    private:
        ComponentType m_type;
        std::string m_name;
        std::shared_ptr<Components::JComponentData> m_jcompData = nullptr;
    };

    class ComponentBank{
    public:
        static void addToCollection(std::string collectionName, ComponentBankElement element);
        typedef std::unordered_map<std::string, std::vector<ComponentBankElement>> BankVault;
        static const BankVault& getVault();

        static const std::shared_ptr<Components::JComponentData> getJCompData(const std::string& collection, const std::string& elementName);

        static const std::vector<ComponentBankElement>& getCollection(const std::string& collection);

        static void loadFromJson(const std::string& filepath);
        static void loadMultiFromJson(const std::string& filepath);

    private:
        static BankVault m_vault;
    };
}