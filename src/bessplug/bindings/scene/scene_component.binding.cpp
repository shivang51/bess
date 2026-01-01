
#include "scene/scene_state/components/scene_component.h"
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>

namespace py = pybind11;

class PySceneComponent : public Bess::Canvas::SceneComponent,
                         public py::trampoline_self_life_support {
  public:
    PySceneComponent() = default;

    void setCompDefHash(uint64_t hash) { m_compDefHash = hash; }
    uint64_t getCompDefHash() const { return m_compDefHash; }

  private:
    uint64_t m_compDefHash = false;
};

void bind_scene_component(py::module_ &m) {
    py::class_<Bess::Canvas::SceneComponent,
               PySceneComponent,
               py::smart_holder>(m, "SceneComponent")
        .def(py::init<>())
        .def("set_comp_def_hash",
             [](Bess::Canvas::SceneComponent &self, size_t h) {
                 static_cast<PySceneComponent &>(self).setCompDefHash(h);
             })
        .def("get_comp_def_hash",
             [](const Bess::Canvas::SceneComponent &self) {
                 return static_cast<const PySceneComponent &>(self).getCompDefHash();
             });
}
