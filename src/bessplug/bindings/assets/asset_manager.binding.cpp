#include "bess_core/asset_manager/asset_manager.h"
#include "bess_core/asset_manager/asset_id.h"
#include "bess_core/g_app_context.h"
#include "bess_wgpu/wgpu_texture.h"

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
        return *it;
    }

  public:
    static AssetID<Bess::Wgpu::WgpuTexture, 1>
    register_texture_asset(const std::string &path) {
        py::gil_scoped_acquire acquire;
        std::string_view safe_path = intern_string(path);
        return AssetID<Bess::Wgpu::WgpuTexture, 1>(intern_string(path));
    }

    static std::shared_ptr<Bess::Wgpu::WgpuTexture>
    get_texture_asset(const AssetID<Bess::Wgpu::WgpuTexture, 1> &id) {
        py::gil_scoped_acquire acquire;
        if (id.paths.empty()) {
            return nullptr;
        }
        std::string path{id.paths[0]};
        auto tex = std::make_shared<Bess::Wgpu::WgpuTexture>(path);
        tex->init();
        return tex;
    }
};

void bind_asset_manager(py::module_ &m) {
    py::class_<AssetID<Bess::Wgpu::WgpuTexture, 1>>(m, "TextureAssetID")
        .def(py::init<const std::string &>())
        .def_readonly("paths",
                      &AssetID<Bess::Wgpu::WgpuTexture, 1>::paths,
                      "Get the paths of the texture asset")
        .def("__repr__", [](const AssetID<Bess::Wgpu::WgpuTexture, 1> &self) {
            std::string repr = "TextureAssetID(";
            if (!self.paths.empty()) {
                repr += py::repr(py::cast(self.paths[0])).cast<std::string>();
            }
            repr += ")";
            return repr;
        });

    py::class_<PyAssetManager>(m, "AssetManager")
        .def_static("register_texture_asset",
                    &PyAssetManager::register_texture_asset,
                    py::arg("path"),
                    "Register a texture asset and return its AssetID")
        .def_static("get_texture_asset",
                    &PyAssetManager::get_texture_asset,
                    py::arg("asset_id"),
                    "Get a texture asset by its AssetID");
}
