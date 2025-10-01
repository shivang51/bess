#pragma once

#include <array>
#include <vulkan/vulkan.h>
#include <glm.hpp>

namespace Bess::Renderer2D::Vulkan {

    struct GridVertex {
        glm::vec3 position;    // location 0
        glm::vec2 texCoord;    // location 1
        int fragId;           // location 2
        glm::vec4 fragColor;   // location 3
        int ar;               // location 4

        static VkVertexInputBindingDescription getBindingDescription() {
            VkVertexInputBindingDescription bindingDescription{};
            bindingDescription.binding = 0;
            bindingDescription.stride = sizeof(GridVertex);
            bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
            return bindingDescription;
        }

        static std::array<VkVertexInputAttributeDescription, 5> getAttributeDescriptions() {
            std::array<VkVertexInputAttributeDescription, 5> attributeDescriptions{};

            // Position
            attributeDescriptions[0].binding = 0;
            attributeDescriptions[0].location = 0;
            attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
            attributeDescriptions[0].offset = offsetof(GridVertex, position);

            // TexCoord
            attributeDescriptions[1].binding = 0;
            attributeDescriptions[1].location = 1;
            attributeDescriptions[1].format = VK_FORMAT_R32G32_SFLOAT;
            attributeDescriptions[1].offset = offsetof(GridVertex, texCoord);

            // FragId
            attributeDescriptions[2].binding = 0;
            attributeDescriptions[2].location = 2;
            attributeDescriptions[2].format = VK_FORMAT_R32_SINT;
            attributeDescriptions[2].offset = offsetof(GridVertex, fragId);

            // FragColor
            attributeDescriptions[3].binding = 0;
            attributeDescriptions[3].location = 3;
            attributeDescriptions[3].format = VK_FORMAT_R32G32B32A32_SFLOAT;
            attributeDescriptions[3].offset = offsetof(GridVertex, fragColor);

            // AR
            attributeDescriptions[4].binding = 0;
            attributeDescriptions[4].location = 4;
            attributeDescriptions[4].format = VK_FORMAT_R32_SINT;
            attributeDescriptions[4].offset = offsetof(GridVertex, ar);

            return attributeDescriptions;
        }
    };

    struct UniformBufferObject {
        glm::mat4 mvp;
    };

    // std140-compatible layout for the fragment UBO
    struct GridUniforms {
        float zoom;              // offset 0
        float _pad0;             // pad to 8 for next vec2
        glm::vec2 cameraOffset;  // offset 8
        glm::vec4 gridMinorColor;// offset 16
        glm::vec4 gridMajorColor;// offset 32
        glm::vec4 axisXColor;    // offset 48
        glm::vec4 axisYColor;    // offset 64
        glm::vec2 resolution;    // offset 80
        glm::vec2 _pad1;         // pad to 96 (multiple of 16)
    };

    struct QuadInstance {
        glm::vec3 position;     // location 2
        glm::vec4 color;        // location 3
        glm::vec4 borderRadius; // location 4
        glm::vec4 borderColor;  // location 5
        glm::vec4 borderSize;   // location 6
        glm::vec2 size;         // location 7
        int id;                 // location 8
        int isMica;             // location 9
        int texSlotIdx;         // location 10
        int _pad;               // padding to align next vec4
        glm::vec4 texData;      // location 11
    };

} // namespace Bess::Renderer2D::Vulkan
