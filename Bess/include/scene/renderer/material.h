#pragma once

#include "primitive_vertex.h"
#include "scene/renderer/path.h"
#include "vulkan_texture.h"
#include <cstdint>
#include <memory>

namespace Bess::Renderer {
    struct QuadMaterial {
        Vulkan::QuadInstance instance;
        std::shared_ptr<Vulkan::VulkanTexture> texture = nullptr;
    };

    struct CircleMaterial {
        Vulkan::CircleInstance instance;
    };

    struct PathMaterial {
        std::shared_ptr<Path> path = nullptr;
    };

    struct GridMaterial {
        glm::vec3 position;
        glm::vec2 size;
        int id;
        Vulkan::GridUniforms uniforms;
    };

    struct Material2D {
        enum class MaterialType : uint8_t {
            quad,
            circle,
            path,
            grid
        } type;

        union {
            QuadMaterial quad;
            CircleMaterial circle;
            PathMaterial path;
            GridMaterial grid;
        };

        float z = 0.f;
        float alpha = 1.f;

        Material2D() : type(MaterialType::quad), quad{} {}
        explicit Material2D(MaterialType type, float z, float alpha) : type(type), z(z), alpha(alpha) {
        }

        ~Material2D() {
            destroyActive();
        }

        Material2D(const Material2D &other)
            : type(other.type), z(other.z), alpha(other.alpha) {
            constructFrom(other);
        }

        Material2D(Material2D &&other) noexcept
            : type(other.type), z(other.z), alpha(other.alpha) {
            moveFrom(std::move(other));
        }

        Material2D &operator=(const Material2D &other) {
            if (this == &other)
                return *this;
            destroyActive();
            type = other.type;
            z = other.z;
            alpha = other.alpha;
            constructFrom(other);
            return *this;
        }

        Material2D &operator=(Material2D &&other) noexcept {
            if (this == &other)
                return *this;
            destroyActive();
            type = other.type;
            type = other.type;
            z = other.z;
            alpha = other.alpha;
            moveFrom(std::move(other));
            return *this;
        }

      private:
        void destroyActive() {
            switch (type) {
            case MaterialType::quad:
                quad.~QuadMaterial();
                break;
            case MaterialType::circle:
                circle.~CircleMaterial();
                break;
            case MaterialType::path:
                path.~PathMaterial();
                break;
            case MaterialType::grid:
                grid.~GridMaterial();
                break;
            }
        }

        void constructFrom(const Material2D &other) {
            switch (other.type) {
            case MaterialType::quad:
                new (&quad) QuadMaterial(other.quad);
                break;
            case MaterialType::circle:
                new (&circle) CircleMaterial(other.circle);
                break;
            case MaterialType::path:
                new (&path) PathMaterial(other.path);
                break;
            case MaterialType::grid:
                new (&grid) GridMaterial(other.grid);
                break;
            }
        }

        void moveFrom(Material2D &&other) {
            switch (other.type) {
            case MaterialType::quad:
                new (&quad) QuadMaterial(std::move(other.quad));
                break;
            case MaterialType::circle:
                new (&circle) CircleMaterial(std::move(other.circle));
                break;
            case MaterialType::path:
                new (&path) PathMaterial(std::move(other.path));
                break;
            case MaterialType::grid:
                new (&grid) GridMaterial(std::move(other.grid));
                break;
            }
        }
    };

}; // namespace Bess::Renderer
