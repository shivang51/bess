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

    struct GridUniforms {
        float zoom;
        glm::vec2 cameraOffset;
        glm::vec4 gridMinorColor;
        glm::vec4 gridMajorColor;
        glm::vec4 axisXColor;
        glm::vec4 axisYColor;
        glm::vec2 resolution;
    };

} // namespace Bess::Renderer2D::Vulkan
