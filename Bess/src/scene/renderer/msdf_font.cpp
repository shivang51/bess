#include "scene/renderer/msdf_font.h"
#include "common/log.h"
#include "json.hpp"

#include <fstream>

namespace Bess::Renderer2D {

    MsdfFont::MsdfFont(const std::string &path, const std::string& jsonFileName, float fontSize) : m_fontSize(fontSize) {
        loadFont(path, jsonFileName);
    }

    MsdfFont::~MsdfFont() = default;

    void MsdfFont::loadFont(const std::string &path, const std::string& jsonFileName) {

        std::filesystem::path path_ = path;
        std::ifstream inFile(path_ / jsonFileName);
        if (!inFile.is_open()) {
            BESS_ERROR("Failed to open MSDF font json file: {}", path);
            return;
        }
        nlohmann::json charData;
        inFile >> charData;


        if(!charData.contains("pages")){
            BESS_ERROR("Invalid MSDF font json file: {}\n\tUnable to locate pages", path);
            return;
        }

        if(!charData.contains("chars")){
            BESS_ERROR("Invalid MSDF font json file: {}\n\tUnable to locate chars", path);
            return;
        }


        auto pages = charData["pages"].get<std::vector<std::string>>();
        BESS_INFO("Loaded MSDF font json data from: {}", (path_ / jsonFileName).string());
        // just using first for now
        auto pageName = pages[0];
        auto pagePath = path_ / pageName;
        m_fontTextureAtlas = std::make_shared<Gl::Texture>(pagePath.string());

        auto chars = charData["chars"];
        for (const auto &c : chars) {
            MsdfCharacter character;
            character.offset = glm::vec2(c["xoffset"], c["yoffset"]);
            character.size = glm::vec2(c["width"], c["height"]);
            character.texPos = glm::vec2(c["x"], c["y"]);
            character.advance = c["xadvance"];
            character.character = c["char"].get<std::string>()[0];
            auto subTex = std::make_shared<Gl::SubTexture>();
            character.subTexture = subTex;

            subTex->calcCoordsFrom(m_fontTextureAtlas, character.texPos, character.size);
            m_charData[character.character] = character;
        }
    }

    float MsdfFont::getScale(float size) const {
        return size / m_fontSize;
    }

    MsdfCharacter MsdfFont::getCharacterData(char c) const {
        if (m_charData.find(c) != m_charData.end()) {
            return m_charData.at(c);
        }
        BESS_ERROR("Character '{}' not found in MSDF font data. Returning for *", c);
        return m_charData.at('*');
    }

    std::shared_ptr<Gl::Texture> MsdfFont::getTextureAtlas() const {
        return m_fontTextureAtlas;
    }

} // namespace Bess::Renderer2D