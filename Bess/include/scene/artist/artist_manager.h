#pragma once

#include "base_artist.h"
#include <memory>

// Forward declarations
namespace Bess::Canvas {
    class SchematicArtist;
    class NodesArtist;
    class Viewport;
} // namespace Bess::Canvas

namespace Bess::Canvas {

    class ArtistManager {
      public:
        ArtistManager(std::shared_ptr<Viewport> viewport);
        ~ArtistManager();

        void destroy();

        std::shared_ptr<BaseArtist> getCurrentArtist() const;
        std::shared_ptr<SchematicArtist> getSchematicArtist();
        std::shared_ptr<NodesArtist> getNodesArtist();

        void setSchematicMode(bool isSchematic);

      private:
        std::shared_ptr<Viewport> m_viewport;
        std::shared_ptr<SchematicArtist> m_schematicArtist;
        std::shared_ptr<NodesArtist> m_nodesArtist;
        bool m_isSchematicMode;
    };

} // namespace Bess::Canvas
