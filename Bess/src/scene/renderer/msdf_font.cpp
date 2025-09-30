#include "scene/renderer/msdf_font.h"
#include "common/log.h"
#include "scene/renderer/vulkan/vulkan_texture.h"

#include <filesystem>
#include <fstream>

namespace Bess::Renderer2D {

    MsdfFont::MsdfFont(const std::string &path, const std::string &jsonFileName) {
        // This constructor is deprecated - use the one with device parameter
        BESS_WARN("MsdfFont constructor without device is deprecated. Use MsdfFont(path, jsonFileName, device) instead.");
    }

    MsdfFont::MsdfFont(const std::string &path, const std::string &jsonFileName, Vulkan::VulkanDevice& device) : m_device(&device) {
        loadFont(path, jsonFileName, device);
    }

    MsdfFont::~MsdfFont() = default;

    void MsdfFont::loadFont(const std::string &path, const std::string &fileName) const {
        // This method is deprecated - use the one with device parameter
        BESS_WARN("MsdfFont::loadFont without device is deprecated. Use loadFont(path, fileName, device) instead.");
    }

    void MsdfFont::loadFont(const std::string &path, const std::string &fileName, Vulkan::VulkanDevice& device) {
        m_device = &device;
        std::filesystem::path path_ = path;
        const std::string jsonFileName = fileName + ".json";
        std::ifstream inFile(path_ / jsonFileName);
        if (!inFile.is_open()) {
            BESS_ERROR("Failed to open MSDF font json file: {}", path);
            return;
        }

        Json::Value charData;
        inFile >> charData;
        const std::string pngFilePath = path_ / (fileName + ".png");

        if (!isValidJson(charData)) {
            BESS_ERROR("Invalid MSDF font json file: {}\n\tUnable to locate required fields", path);
            assert(false);
            return;
        }

        // Create Vulkan texture from PNG file
        m_fontTextureAtlas = std::make_shared<Vulkan::VulkanTexture>(device, pngFilePath);
        BESS_INFO("Loaded MSDF font texture atlas from: {}", pngFilePath);

        m_fontSize = charData["atlas"]["size"].asFloat();
        m_lineHeight = charData["metrics"]["lineHeight"].asFloat();

        const Json::Value &chars = charData["glyphs"];
        std::unordered_map<char, MsdfCharacter> glyphs;
        size_t maxAscii = 0;

        for (const auto &c : chars) {
            if (!c.isObject())
                continue; // Skip if an element is not a valid object.

            MsdfCharacter character;

            character.advance = c.get("advance", 0.f).asFloat();

            if (c.isMember("atlasBounds")) {
                auto atlasBounds = getBounds(c["atlasBounds"]);

                auto atlasPos = glm::vec2(atlasBounds.left, m_fontTextureAtlas->getHeight() - atlasBounds.top);
                auto atlasSize = glm::vec2(atlasBounds.right - atlasBounds.left, atlasBounds.top - atlasBounds.bottom);
                auto atlasMax = atlasPos + atlasSize;
                auto subTex = std::make_shared<Vulkan::VulkanSubTexture>(m_fontTextureAtlas, atlasPos, atlasMax);
                character.subTexture = subTex;
            }

            if (c.isMember("planeBounds")) {
                Bounds planeBounds = getBounds(c["planeBounds"]);
                character.offset = glm::vec2(planeBounds.left, planeBounds.bottom);
                character.size = glm::vec2(planeBounds.right - planeBounds.left, planeBounds.top - planeBounds.bottom);
            }

            character.character = (char)c.get("unicode", 32).asUInt64();

            glyphs[character.character] = character;
            maxAscii = std::max(maxAscii, (size_t)character.character);
        }

        m_charTable.resize(maxAscii + 1);

        for (auto &[ch, data] : glyphs) {
            m_charTable[(size_t)ch] = data;
        }

        BESS_TRACE("[MsdfFont] Made lookup table of size {} characters", maxAscii);
    }

    float MsdfFont::getScale(float size) const {
        return size / m_fontSize;
    }

    float MsdfFont::getLineHeight() const {
        return m_lineHeight;
    }

    const MsdfCharacter &MsdfFont::getCharacterData(char c) const {
        return m_charTable[(size_t)c];
    }

    std::shared_ptr<Vulkan::VulkanTexture> MsdfFont::getTextureAtlas() const {
        return m_fontTextureAtlas;
    }

    bool MsdfFont::isValidJson(const Json::Value &json) const {
        return json.isMember("atlas") && json.isMember("metrics") && json.isMember("glyphs");
    }

    MsdfFont::Bounds MsdfFont::getBounds(const Json::Value &val) const {
        Bounds bounds;
        bounds.left = val["left"].as<float>();
        bounds.right = val["right"].as<float>();
        bounds.top = val["top"].as<float>();
        bounds.bottom = val["bottom"].as<float>();
        return bounds;
    }
} // namespace Bess::Renderer2D
