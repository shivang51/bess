#include "project_file.h"
#include "json.hpp"
#include "common/helpers.h"
#include "components_manager/components_manager.h"

#include "components/jcomponent.h"
#include "components/input_probe.h"
#include "components/output_probe.h"

#include <fstream>
#include <iostream>

namespace Bess {
    ProjectFile::ProjectFile(const std::string& path)
    {
        std::cout << "[+] Opening project " << path << std::endl;
        m_path = path;
        decode();
        m_saved = true;
    }

    ProjectFile::~ProjectFile()
    {
    }

    void ProjectFile::save()
    {
        if (m_saved) return;
        std::cout << "[+] Saving project " << m_path << std::endl;
        auto data = encode();
        std::ofstream o(m_path);
        o << std::setw(4) << data << std::endl;
        m_saved = true;
    }

    void ProjectFile::update(const Simulator::TComponents components)
    {
        m_saved = false;
    }

    const std::string& ProjectFile::getName() const
    {
        return m_name;
    }

    void ProjectFile::setName(const std::string& name)
    {
        m_name = name;
    }

    const std::string& ProjectFile::getPath() const
    {
        return m_path;
    }

    void ProjectFile::setPath(const std::string& path)
    {
        m_path = path;
    }

    nlohmann::json ProjectFile::encode()
    {
        nlohmann::json data;
        data["name"] = m_name;
        for (auto& compId : Simulator::ComponentsManager::renderComponenets) {
            auto& ent = Simulator::ComponentsManager::components[compId];
            switch (ent->getType())
            {
            case Bess::Simulator::ComponentType::inputProbe:
            {
                auto comp = (Bess::Simulator::Components::InputProbe*)ent.get();
                data["components"].emplace_back(comp->toJson());
            }
            break;
            case Bess::Simulator::ComponentType::outputProbe:
            {
                auto comp = (Bess::Simulator::Components::OutputProbe*)ent.get();
                data["components"].emplace_back(comp->toJson());
            }
            break;
            case Bess::Simulator::ComponentType::jcomponent: {

                auto comp = (Bess::Simulator::Components::JComponent*)ent.get();
                data["components"].emplace_back(comp->toJson());
            }
            break;
            default:
                break;
            }
        }

        return data;
    }

    void ProjectFile::decode()
    {
        std::ifstream file(m_path);
        nlohmann::json data = nlohmann::json::parse(file);
        m_name = data["name"];
        auto& components = data["components"];

        for (auto& comp : components) {
            auto compType = Common::Helpers::intToCompType(comp["type"]);

            switch (compType)
            {
            case Bess::Simulator::ComponentType::inputProbe:
                Simulator::Components::InputProbe::fromJson(comp);
                break;
            case Bess::Simulator::ComponentType::outputProbe:
                Simulator::Components::OutputProbe::fromJson(comp);
                break;
            case Bess::Simulator::ComponentType::jcomponent:
                Simulator::Components::JComponent::fromJson(comp);
                break;
            default:
                break;
            }
        }
    }
}