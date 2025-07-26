#include "scene/renderer/msdf_font.h"
#include "common/log.h"
#include "json/json.h"

#include <fstream>
#include <filesystem>

namespace Bess::Renderer2D {

    MsdfFont::MsdfFont(const std::string &path, const std::string& jsonFileName, float fontSize) : m_fontSize(fontSize) {
        loadFont(path, jsonFileName);
    }

    MsdfFont::~MsdfFont() = default;

    void MsdfFont::loadFont(const std::string &path, const std::string& jsonFileName) {
        m_lineHeight = 34.f;
        std::filesystem::path path_ = path;
        std::ifstream inFile(path_ / jsonFileName);
        if (!inFile.is_open()) {
            BESS_ERROR("Failed to open MSDF font json file: {}", path);
            return;
        }
        Json::Value charData;
        inFile >> charData;


        if(!charData.isMember("pages")){
            BESS_ERROR("Invalid MSDF font json file: {}\n\tUnable to locate pages", path);
            return;
        }

        if(!charData.isMember("chars")){
            BESS_ERROR("Invalid MSDF font json file: {}\n\tUnable to locate chars", path);
            return;
        }


        // Check if the "pages" member exists and is an array.
        if (charData.isMember("pages") && charData["pages"].isArray()) {
            const Json::Value &pagesArray = charData["pages"];
            if (!pagesArray.empty()) {
                // Get the first page name.
                std::string pageName = pagesArray[0].asString();
                auto pagePath = path_ / pageName;
                m_fontTextureAtlas = std::make_shared<Gl::Texture>(pagePath.string());
                BESS_INFO("Loaded MSDF font texture atlas from: {}", pagePath.string());
            }
        }

        if (charData.isMember("chars") && charData["chars"].isArray()) {
            const Json::Value &chars = charData["chars"];
            for (const auto &c : chars) {
                if (!c.isObject())
                    continue; // Skip if an element is not a valid object.

                MsdfCharacter character;
                character.offset = glm::vec2(c.get("xoffset", 0.f).asFloat(), c.get("yoffset", 0.f).asFloat());
                character.size = glm::vec2(c.get("width", 0.f).asFloat(), c.get("height", 0.f).asFloat());
                character.texPos = glm::vec2(c.get("x", 0.f).asFloat(), c.get("y", 0.f).asFloat());
                character.advance = c.get("xadvance", 0.f).asFloat();

                std::string charStr = c.get("char", "").asString();
                if (!charStr.empty()) {
                    character.character = charStr[0];
                }

                auto subTex = std::make_shared<Gl::SubTexture>();
                character.subTexture = subTex;

                subTex->calcCoordsFrom(m_fontTextureAtlas, character.texPos, character.size);
                m_charData[character.character] = character;
            }
        }
    }

    float MsdfFont::getScale(float size) const {
        return size / m_fontSize;
    }

    float MsdfFont::getLineHeight() const {
        return m_lineHeight;
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