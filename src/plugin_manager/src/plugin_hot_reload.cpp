#include "plugin_hot_reload.h"
#include <cctype>
#include <functional>
#include <optional>
#include <pybind11/eval.h>
#include <sstream>
#include <stdexcept>

namespace {
    namespace py = pybind11;

    std::string sanitizeModulePart(std::string value) {
        for (char &ch : value) {
            const auto uch = static_cast<unsigned char>(ch);
            if (!std::isalnum(uch) && ch != '_') {
                ch = '_';
            }
        }

        if (value.empty() ||
            std::isdigit(static_cast<unsigned char>(value.front()))) {
            value.insert(value.begin(), '_');
        }

        return value;
    }

    bool isPathInside(const std::filesystem::path &path,
                      const std::filesystem::path &root) {
        auto pathIt = path.begin();
        auto rootIt = root.begin();

        for (; rootIt != root.end(); ++rootIt, ++pathIt) {
            if (pathIt == path.end() || *pathIt != *rootIt) {
                return false;
            }
        }

        return true;
    }

    std::optional<std::filesystem::path>
    canonicalPathIfPossible(const std::filesystem::path &path) {
        try {
            return std::filesystem::weakly_canonical(path);
        } catch (const std::filesystem::filesystem_error &) {
            return std::nullopt;
        }
    }

    std::optional<std::string>
    relativePathKey(const std::filesystem::path &path,
                    const std::filesystem::path &root) {
        try {
            return std::filesystem::relative(path, root).generic_string();
        } catch (const std::filesystem::filesystem_error &) {
            return std::nullopt;
        }
    }

    std::string makePluginClassKey(const std::filesystem::path &modulePath,
                                   const std::filesystem::path &root,
                                   const std::string &qualName) {
        const auto relativePath = relativePathKey(modulePath, root);
        if (relativePath.has_value()) {
            return *relativePath + ":" + qualName;
        }
        return modulePath.generic_string() + ":" + qualName;
    }

    bool isLivePatchablePluginClass(const py::object &klass) {
        py::object issubclass =
            py::module_::import("builtins").attr("issubclass");
        py::object simSceneComponentClass =
            py::module_::import("bessplug.api.scene")
                .attr("SimulationSceneComponent");

        try {
            return issubclass(klass, simSceneComponentClass).cast<bool>();
        } catch (const py::error_already_set &) {
            return false;
        }
    }

    bool isClassAttrSetSafe(const std::string &name) {
        return name != "__dict__" && name != "__weakref__" &&
               name != "__classcell__";
    }

    bool isClassAttrDeleteSafe(const std::string &name) {
        const bool isDunder = name.starts_with("__") && name.ends_with("__");
        return !isDunder && isClassAttrSetSafe(name);
    }

    py::object makeClassClosureRebinder() {
        py::dict scope;
        scope["__builtins__"] = py::module_::import("builtins");
        py::exec(R"PY(
import types

def _make_cell(value):
    def inner():
        return value
    return inner.__closure__[0]

def _copy_function_metadata(source, target):
    for name in ("__kwdefaults__", "__qualname__", "__module__", "__doc__"):
        try:
            setattr(target, name, getattr(source, name))
        except Exception:
            pass

    try:
        target.__annotations__ = dict(getattr(source, "__annotations__", {}))
    except Exception:
        pass

    try:
        target.__dict__.update(getattr(source, "__dict__", {}))
    except Exception:
        pass

def _rebind_function_class_cell(fn, old_class, new_class):
    if not isinstance(fn, types.FunctionType):
        return fn

    closure = getattr(fn, "__closure__", None)
    if not closure:
        return fn

    cells = []
    changed = False
    for name, cell in zip(fn.__code__.co_freevars, closure):
        if name != "__class__":
            cells.append(cell)
            continue

        try:
            contents = cell.cell_contents
        except ValueError:
            cells.append(cell)
            continue

        if contents is new_class:
            cells.append(_make_cell(old_class))
            changed = True
        else:
            cells.append(cell)

    if not changed:
        return fn

    rebound = types.FunctionType(
        fn.__code__,
        fn.__globals__,
        fn.__name__,
        fn.__defaults__,
        tuple(cells),
    )
    _copy_function_metadata(fn, rebound)
    return rebound

def _bess_rebind_class_closure(value, old_class, new_class):
    if isinstance(value, staticmethod):
        return staticmethod(
            _rebind_function_class_cell(value.__func__, old_class, new_class)
        )
    if isinstance(value, classmethod):
        return classmethod(
            _rebind_function_class_cell(value.__func__, old_class, new_class)
        )
    if isinstance(value, property):
        return property(
            _rebind_function_class_cell(value.fget, old_class, new_class)
                if value.fget else None,
            _rebind_function_class_cell(value.fset, old_class, new_class)
                if value.fset else None,
            _rebind_function_class_cell(value.fdel, old_class, new_class)
                if value.fdel else None,
            value.__doc__,
        )
    if isinstance(value, types.FunctionType):
        return _rebind_function_class_cell(value, old_class, new_class)
    return value
)PY",
                 scope,
                 scope);
        return scope["_bess_rebind_class_closure"];
    }

    py::object makeLiveInstanceMover() {
        py::dict scope;
        scope["__builtins__"] = py::module_::import("builtins");
        py::exec(R"PY(
import gc

def _bess_move_live_instances(old_class, new_class):
    moved = 0
    failed = 0
    for obj in gc.get_objects():
        if type(obj) is not old_class:
            continue

        try:
            obj.__class__ = new_class
            moved += 1
        except Exception:
            failed += 1
    return moved, failed
)PY",
                 scope,
                 scope);
        return scope["_bess_move_live_instances"];
    }

    void patchLiveClassObject(const py::object &oldClass,
                              const py::object &newClass) {
        py::module_ builtins = py::module_::import("builtins");
        py::object setAttr = builtins.attr("setattr");
        py::object delAttr = builtins.attr("delattr");
        py::object rebindClassClosure = makeClassClosureRebinder();

        py::dict oldDict = oldClass.attr("__dict__");
        py::dict newDict = newClass.attr("__dict__");
        py::list oldAttrNames = py::list(oldDict.attr("keys")());

        for (py::handle oldAttrName : oldAttrNames) {
            const auto name = py::str(oldAttrName).cast<std::string>();
            if (isClassAttrDeleteSafe(name) && !newDict.contains(oldAttrName)) {
                delAttr(oldClass, oldAttrName);
            }
        }

        for (const auto &item : newDict) {
            const auto name = py::str(item.first).cast<std::string>();
            if (!isClassAttrSetSafe(name)) {
                continue;
            }
            py::object attr = py::reinterpret_borrow<py::object>(item.second);
            setAttr(oldClass,
                    item.first,
                    rebindClassClosure(attr, oldClass, newClass));
        }
    }
} // namespace

namespace Bess::Plugins::HotReload {
    std::string makePluginModuleName(const std::filesystem::path &pluginPath) {
        const auto normalized =
            std::filesystem::absolute(pluginPath).lexically_normal().string();
        std::ostringstream name;
        name << "bess_plugin_" << sanitizeModulePart(pluginPath.stem().string())
             << "_" << std::hex << std::hash<std::string>{}(normalized);
        return name.str();
    }

    void putSysPathFirst(const std::filesystem::path &path) {
        py::module_ sys = py::module_::import("sys");
        py::list sysPath = sys.attr("path");
        const auto pathString = path.string();

        while (sysPath.contains(pathString)) {
            sysPath.attr("remove")(pathString);
        }
        sysPath.attr("insert")(0, pathString);
    }

    py::dict collectLivePluginClasses(const std::filesystem::path &root) {
        const auto canonicalRoot = canonicalPathIfPossible(root);
        py::dict classes;
        if (!canonicalRoot.has_value()) {
            return classes;
        }

        py::object isClass = py::module_::import("inspect").attr("isclass");
        py::dict modules = py::module_::import("sys").attr("modules");
        py::list moduleNames = py::list(modules.attr("keys")());

        for (py::handle moduleName : moduleNames) {
            py::object module = modules[moduleName];
            if (module.is_none()) {
                continue;
            }

            if (!py::hasattr(module, "__file__")) {
                continue;
            }

            py::object moduleFile = module.attr("__file__");
            if (moduleFile.is_none()) {
                continue;
            }

            const auto canonicalModulePath = canonicalPathIfPossible(
                py::str(moduleFile).cast<std::string>());
            if (!canonicalModulePath.has_value()) {
                continue;
            }

            if (!isPathInside(*canonicalModulePath, *canonicalRoot)) {
                continue;
            }

            const auto moduleNameString =
                py::str(moduleName).cast<std::string>();
            py::dict moduleDict = module.attr("__dict__");

            for (const auto &item : moduleDict) {
                py::object attr =
                    py::reinterpret_borrow<py::object>(item.second);
                if (!isClass(attr).cast<bool>()) {
                    continue;
                }

                if (!isLivePatchablePluginClass(attr)) {
                    continue;
                }

                if (!py::hasattr(attr, "__module__") ||
                    !py::hasattr(attr, "__qualname__")) {
                    continue;
                }

                if (py::str(attr.attr("__module__")).cast<std::string>() !=
                    moduleNameString) {
                    continue;
                }

                const auto qualName =
                    py::str(attr.attr("__qualname__")).cast<std::string>();
                const auto classKey = makePluginClassKey(
                    *canonicalModulePath, *canonicalRoot, qualName);
                classes[py::str(classKey)] = attr;
            }
        }

        return classes;
    }

    py::dict removeCachedModules(const std::filesystem::path &root) {
        const auto canonicalRoot = canonicalPathIfPossible(root);
        py::dict removedModules;
        if (!canonicalRoot.has_value()) {
            return removedModules;
        }

        py::dict modules = py::module_::import("sys").attr("modules");
        py::list moduleNames = py::list(modules.attr("keys")());

        for (py::handle moduleName : moduleNames) {
            py::object module = modules[moduleName];
            if (module.is_none()) {
                continue;
            }

            if (!py::hasattr(module, "__file__")) {
                continue;
            }

            py::object moduleFile = module.attr("__file__");
            if (moduleFile.is_none()) {
                continue;
            }

            const auto canonicalModulePath = canonicalPathIfPossible(
                py::str(moduleFile).cast<std::string>());
            if (!canonicalModulePath.has_value()) {
                continue;
            }

            if (!isPathInside(*canonicalModulePath, *canonicalRoot)) {
                continue;
            }

            removedModules[moduleName] = module;
            modules.attr("pop")(moduleName, py::none());
        }

        return removedModules;
    }

    void restoreCachedModules(const py::dict &removedModules) {
        py::dict modules = py::module_::import("sys").attr("modules");
        for (const auto &item : removedModules) {
            modules[item.first] = item.second;
        }
    }

    size_t patchLivePluginClasses(const py::dict &oldClasses,
                                  const std::filesystem::path &root) {
        py::dict newClasses = collectLivePluginClasses(root);
        py::object moveLiveInstances = makeLiveInstanceMover();
        size_t patchedClassCount = 0;

        for (const auto &item : oldClasses) {
            if (!newClasses.contains(item.first)) {
                continue;
            }

            py::object oldClass =
                py::reinterpret_borrow<py::object>(item.second);
            py::object newClass = newClasses[item.first];

            py::tuple moveResult =
                moveLiveInstances(oldClass, newClass).cast<py::tuple>();
            const auto failedMoveCount = moveResult[1].cast<size_t>();
            if (failedMoveCount > 0) {
                patchLiveClassObject(oldClass, newClass);
            }
            patchedClassCount++;
        }

        return patchedClassCount;
    }

    py::module_
    importPluginModuleFromFile(const std::filesystem::path &pluginPath,
                               const std::string &moduleName) {
        py::module_ importlibUtil = py::module_::import("importlib.util");
        py::object spec = importlibUtil.attr("spec_from_file_location")(
            moduleName, pluginPath.string());
        if (spec.is_none()) {
            throw std::runtime_error("Could not create import spec for " +
                                     pluginPath.string());
        }

        py::object module = importlibUtil.attr("module_from_spec")(spec);
        py::dict modules = py::module_::import("sys").attr("modules");
        modules[py::str(moduleName)] = module;

        try {
            spec.attr("loader").attr("exec_module")(module);
        } catch (...) {
            modules.attr("pop")(moduleName, py::none());
            throw;
        }

        return py::reinterpret_borrow<py::module_>(module);
    }
} // namespace Bess::Plugins::HotReload
