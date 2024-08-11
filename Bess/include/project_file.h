#pragma once

#include <string>
#include "components_manager/components_manager.h"

#include "json.hpp"

namespace Bess {
    class ProjectFile {
    public:
        ProjectFile();
        ProjectFile(const std::string& path);
        ~ProjectFile();

        void save();
        void update(const Simulator::TComponents components);

        const std::string& getName() const;
        void setName(const std::string& name);


        const std::string& getPath() const;
        void setPath(const std::string& path);

        bool isSaved();
 
    private:
        nlohmann::json encode();
        void decode();
        void browsePath();

    private:
        std::string m_name = "";
        std::string m_path = "";

        bool m_saved = false;
    };
}