#include "asset_manager/asset_manager.h"
#include "asset_manager/asset_id.h"
#include "vulkan_core.h"
#include "vulkan_texture.h"

#include <memory>
#include <pybind11/pybind11.h>

namespace py = pybind11;

using namespace Bess::Assets;

class PyAssetManager {
  private:
    static std::string_view intern_string(const std::string &s) {
        static std::unordered_set<std::string> pool;
        static std::mutex pool_mutex;
        py::gil_scoped_acquire acquire;

        std::lock_guard<std::mutex> lock(pool_mutex);
        auto [it, inserted] = pool.insert(s);
        return *it; // Return a view of the string stored inside the set
    }

  public:
    static AssetID<Bess::Vulkan::VulkanTexture, 1> register_texture_asset(const std::string &path) {
        py::gil_scoped_acquire acquire;
        std::string_view safe_path = intern_string(path);
        return AssetID<Bess::Vulkan::VulkanTexture, 1>(intern_string(path));
    }

    static std::shared_ptr<Bess::Vulkan::VulkanTexture> get_texture_asset(const AssetID<Bess::Vulkan::VulkanTexture, 1> &id) {
        assert(Bess::Vulkan::VulkanCore::instance().getDevice() && "Vulkan device must be initialized before getting texture assets.");
        return AssetManager::instance().get(id);
    }
};

void bind_asset_manager(py::module_ &m) {
    py::class_<AssetID<Bess::Vulkan::VulkanTexture, 1>>(m, "TextureAssetID")
        .def(py::init<const std::string &>())
        .def_readonly("paths", &AssetID<Bess::Vulkan::VulkanTexture, 1>::paths, "Get the paths of the texture asset")
        .def("__repr__", [](const AssetID<Bess::Vulkan::VulkanTexture, 1> &self) {
            std::string repr = "TextureAssetID(";
            repr += py::repr(py::cast(self.paths[0])).cast<std::string>();
            repr += ")";
            return repr;
        });

    py::class_<PyAssetManager>(m, "AssetManager")
        .def_static("register_texture_asset", &PyAssetManager::register_texture_asset, py::arg("path"),
                    "Register a texture asset and return its AssetID")
        .def_static("get_texture_asset", &PyAssetManager::get_texture_asset, py::arg("asset_id"),
                    "Get a texture asset by its AssetID");
}
