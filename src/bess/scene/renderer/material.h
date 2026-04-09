#pragma once

#include "primitive_vertex.h"
#include "scene/renderer/path.h"
#include "vulkan_texture.h"
#include <cstdint>
#include <memory>

namespace Bess::Renderer {
    struct PrimitiveMaterial {
        Vulkan::PrimitiveInstance instance;
        std::shared_ptr<Vulkan::VulkanTexture> texture = nullptr;
    };

    struct PathMaterial {
        std::shared_ptr<Path> path = nullptr;
    };

    struct GridMaterial {
        glm::vec3 position;
        glm::vec2 size;
        uint64_t id;
        Vulkan::GridUniforms uniforms;
    };

    struct Material2D {
        enum class MaterialType : uint8_t {
            primitive,
            path,
            grid
        } type;

        union {
            PrimitiveMaterial primitive;
            PathMaterial path;
            GridMaterial grid;
        };

        float z = 0.f;
        float alpha = 1.f;

        Material2D() : type(MaterialType::primitive), primitive{} {}
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
            case MaterialType::primitive:
                primitive.~PrimitiveMaterial();
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
            case MaterialType::primitive:
                new (&primitive) PrimitiveMaterial(other.primitive);
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
            case MaterialType::primitive:
                new (&primitive) PrimitiveMaterial(std::move(other.primitive));
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
