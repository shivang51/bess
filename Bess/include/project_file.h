#pragma once

#include "scene/scene_serializer.h"
#include "simulation_engine_serializer.h"
#include "json.hpp"
#include <string>

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
        void encodeAndSave();
        void decode();
        void browsePath();

        void patchFile();

      private:
        std::string m_name = "";
        std::string m_path = "";
        
        bool m_saved = false;
        
        SimEngine::SimEngineSerializer m_simEngineSerializer;
        SceneSerializer m_sceneSerializer;
    };
} // namespace Bess
