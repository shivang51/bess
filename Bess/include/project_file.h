#pragma once

#include <string>

#include "json.hpp"

namespace Bess {
    class ProjectFile {
      public:
        ProjectFile();
        ProjectFile(const std::string &path);
        ~ProjectFile();

        void save();
        // void update(const Simulator::TComponents components);

        const std::string &getName() const;
        std::string &getNameRef();
        void setName(const std::string &name);

        const std::string &getPath() const;
        void setPath(const std::string &path);

        bool isSaved();

      private:
        nlohmann::json encode();
        void decode();
        void browsePath();

        void patchFile();

      private:
        std::string m_name = "";
        std::string m_path = "";

        bool m_saved = false;
    };
} // namespace Bess
