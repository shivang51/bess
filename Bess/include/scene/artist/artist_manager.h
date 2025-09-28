#pragma once

#include "base_artist.h"
#include "scene/scene.h"
#include <memory>

// Forward declarations
namespace Bess::Canvas {
    class SchematicArtist;
    class NodesArtist;
} // namespace Bess::Canvas

namespace Bess::Canvas {

    class ArtistManager {
      public:
        ArtistManager(Scene *scene);
        ~ArtistManager() = default;

        std::shared_ptr<BaseArtist> getCurrentArtist();
        std::shared_ptr<SchematicArtist> getSchematicArtist();
        std::shared_ptr<NodesArtist> getNodesArtist();

        void setSchematicMode(bool isSchematic);

      private:
        Scene *m_scene;
        std::shared_ptr<SchematicArtist> m_schematicArtist;
        std::shared_ptr<NodesArtist> m_nodesArtist;
        bool m_isSchematicMode;
    };

} // namespace Bess::Canvas
