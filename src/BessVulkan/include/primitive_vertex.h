#pragma once

#include "fwd.hpp"
#include <array>
#include <glm.hpp>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

namespace Bess::Vulkan {

    struct GridVertex {
        glm::vec3 position;  // location 0
        glm::vec2 texCoord;  // location 1
        glm::uvec2 fragId;   // location 2
        glm::vec4 fragColor; // location 3
        int ar;              // location 4

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
            attributeDescriptions[2].format = VK_FORMAT_R32G32_UINT;
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
        glm::mat4 ortho;
    };

    // std140-compatible layout for the fragment UBO
    struct GridUniforms {
        float zoom;               // offset 0
        float _pad0;              // pad to 8 for next vec2
        glm::vec2 cameraOffset;   // offset 8
        glm::vec4 gridMinorColor; // offset 16
        glm::vec4 gridMajorColor; // offset 32
        glm::vec4 axisXColor;     // offset 48
        glm::vec4 axisYColor;     // offset 64
        glm::vec2 resolution;     // offset 80
        glm::vec2 _pad1;          // pad to 96 (multiple of 16)
    };

    struct QuadInstance {
        glm::vec3 position;     // location 2
        glm::vec4 color;        // location 3
        glm::vec4 borderRadius; // location 4
        glm::vec4 borderColor;  // location 5
        glm::vec4 borderSize;   // location 6
        glm::vec2 size;         // location 7
        glm::uvec2 id;          // location 8
        int isMica;             // location 9
        int texSlotIdx;         // location 10
        int _pad;               // padding to align next vec4
        glm::vec4 texData;      // location 11
    };

    struct CircleInstance {
        glm::vec3 position; // location 2
        glm::vec4 color;    // location 3
        float radius;       // location 4
        float innerRadius;  // location 5
        glm::uvec2 id;      // location 6
    };

    struct InstanceVertex {
        glm::vec3 position; // location 2
        glm::vec2 size;     // location 3
        float angle;        // location 4
        glm::vec4 color;    // location 5
        glm::uvec2 id;      // location 6
        int texSlotIdx;     // location 7
        glm::vec4 texData;  // location 8
    };

    // Text uniforms for MSDF rendering
    struct TextUniforms {
        float pxRange; // offset 0
        float _pad0;   // pad to 8 for next vec2
        float _pad1;   // pad to 12
        float _pad2;   // pad to 16 (multiple of 16)
    };

    // Common vertex structure for path rendering
    struct CommonVertex {
        glm::vec3 position; // location 0
        glm::vec4 color;    // location 1
        glm::vec2 texCoord; // location 2
        glm::uvec2 id;      // location 3
        int texSlotIdx = 0; // location 4

        static VkVertexInputBindingDescription getBindingDescription() {
            VkVertexInputBindingDescription bindingDescription{};
            bindingDescription.binding = 0;
            bindingDescription.stride = sizeof(CommonVertex);
            bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
            return bindingDescription;
        }

        static std::array<VkVertexInputAttributeDescription, 5> getAttributeDescriptions() {
            std::array<VkVertexInputAttributeDescription, 5> attributeDescriptions{};

            // Position
            attributeDescriptions[0].binding = 0;
            attributeDescriptions[0].location = 0;
            attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
            attributeDescriptions[0].offset = offsetof(CommonVertex, position);

            // Color
            attributeDescriptions[1].binding = 0;
            attributeDescriptions[1].location = 1;
            attributeDescriptions[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
            attributeDescriptions[1].offset = offsetof(CommonVertex, color);

            // TexCoord
            attributeDescriptions[2].binding = 0;
            attributeDescriptions[2].location = 2;
            attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
            attributeDescriptions[2].offset = offsetof(CommonVertex, texCoord);

            // ID
            attributeDescriptions[3].binding = 0;
            attributeDescriptions[3].location = 3;
            attributeDescriptions[3].format = VK_FORMAT_R32G32_UINT;
            attributeDescriptions[3].offset = offsetof(CommonVertex, id);

            // TexSlotIdx
            attributeDescriptions[4].binding = 0;
            attributeDescriptions[4].location = 4;
            attributeDescriptions[4].format = VK_FORMAT_R32_SINT;
            attributeDescriptions[4].offset = offsetof(CommonVertex, texSlotIdx);

            return attributeDescriptions;
        }
    };

    // Instance data for path rendering
    struct PathInstance {
        glm::vec3 position; // location 5 - instance position
        glm::vec3 scale;    // location 6 - instance scale
        glm::vec4 color;    // location 7 - instance color
        glm::uvec2 id;      // location 8 - instance id

        static VkVertexInputBindingDescription getBindingDescription() {
            VkVertexInputBindingDescription bindingDescription{};
            bindingDescription.binding = 1;
            bindingDescription.stride = sizeof(PathInstance);
            bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
            return bindingDescription;
        }

        static std::array<VkVertexInputAttributeDescription, 4> getAttributeDescriptions() {
            std::array<VkVertexInputAttributeDescription, 4> attributeDescriptions{};

            // Position
            attributeDescriptions[0].binding = 1;
            attributeDescriptions[0].location = 5;
            attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
            attributeDescriptions[0].offset = offsetof(PathInstance, position);

            // Scale
            attributeDescriptions[1].binding = 1;
            attributeDescriptions[1].location = 6;
            attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
            attributeDescriptions[1].offset = offsetof(PathInstance, scale);

            // Color
            attributeDescriptions[2].binding = 1;
            attributeDescriptions[2].location = 7;
            attributeDescriptions[2].format = VK_FORMAT_R32G32B32A32_SFLOAT;
            attributeDescriptions[2].offset = offsetof(PathInstance, color);

            // ID
            attributeDescriptions[3].binding = 1;
            attributeDescriptions[3].location = 8;
            attributeDescriptions[3].format = VK_FORMAT_R32G32_UINT;
            attributeDescriptions[3].offset = offsetof(PathInstance, id);

            return attributeDescriptions;
        }
    };

    struct FillInstance {
        glm::vec3 translation; // location 5
        glm::vec2 scale;       // location 6
        glm::vec4 color;       // location 7
        glm::uvec2 pickId;     // location 8

        static VkVertexInputBindingDescription getBindingDescription() {
            VkVertexInputBindingDescription bd{};
            bd.binding = 1;
            bd.stride = sizeof(FillInstance);
            bd.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
            return bd;
        }

        static std::array<VkVertexInputAttributeDescription, 4> getAttributeDescriptions() {
            std::array<VkVertexInputAttributeDescription, 4> a{};
            // translation
            a[0].binding = 1;
            a[0].location = 5;
            a[0].format = VK_FORMAT_R32G32B32_SFLOAT;
            a[0].offset = offsetof(FillInstance, translation);
            // scale
            a[1].binding = 1;
            a[1].location = 6;
            a[1].format = VK_FORMAT_R32G32_SFLOAT;
            a[1].offset = offsetof(FillInstance, scale);
            // scale
            a[2].binding = 1;
            a[2].location = 7;
            a[2].format = VK_FORMAT_R32G32B32A32_SFLOAT;
            a[2].offset = offsetof(FillInstance, color);
            // pick id
            a[3].binding = 1;
            a[3].location = 8;
            a[3].format = VK_FORMAT_R32G32_UINT;
            a[3].offset = offsetof(FillInstance, pickId);
            return a;
        }
    };

} // namespace Bess::Vulkan
