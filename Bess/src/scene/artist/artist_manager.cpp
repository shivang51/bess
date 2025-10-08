#include "scene/artist/artist_manager.h"
#include "common/log.h"
#include "scene/artist/base_artist.h"
#include "scene/artist/nodes_artist.h"
#include "scene/artist/schematic_artist.h"
#include "scene/scene.h"

namespace Bess::Canvas {

    ArtistManager::ArtistManager(std::shared_ptr<Viewport> viewport)
        : m_viewport(viewport),
          m_schematicArtist(std::make_shared<SchematicArtist>(viewport)),
          m_nodesArtist(std::make_shared<NodesArtist>(viewport)), m_isSchematicMode(false) {
    }

    ArtistManager::~ArtistManager() {
        destroy();
    }

    void ArtistManager::destroy() {
        BESS_INFO("[ArtistManager] Destroying");
        BaseArtist::destroyTools();
    }

    std::shared_ptr<BaseArtist> ArtistManager::getCurrentArtist() const {
        return m_isSchematicMode ? std::static_pointer_cast<BaseArtist>(m_schematicArtist) : std::static_pointer_cast<BaseArtist>(m_nodesArtist);
    }

    std::shared_ptr<SchematicArtist> ArtistManager::getSchematicArtist() {
        return m_schematicArtist;
    }

    std::shared_ptr<NodesArtist> ArtistManager::getNodesArtist() {
        return m_nodesArtist;
    }

    void ArtistManager::setSchematicMode(bool isSchematic) {
        m_isSchematicMode = isSchematic;
    }
} // namespace Bess::Canvas
