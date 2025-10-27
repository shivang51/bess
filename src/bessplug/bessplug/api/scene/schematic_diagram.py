from bessplug.api.common.math import Vec2
from bessplug.api.renderer.path import Path
from bessplug.bindings._bindings.scene import SchematicDiagram as NativeSchematicDiagram


class SchematicDiagram:
    def __init__(self, native: NativeSchematicDiagram | None = None):
        self._native = native or NativeSchematicDiagram()

    def get_paths(self) -> list[Path]:
        return [Path(native) for native in self._native.get_paths()]

    def set_paths(self, value: list[Path]):
        native_paths = [path._native for path in value]
        self._native.set_paths(native_paths)

    def add_path(self, path: Path):
        self._native.add_path(path._native)

    @property
    def size(self) -> tuple[float, float]:
        size = self._native.get_size()
        return size.width, size.height

    @size.setter
    def size(self, value: tuple[float, float]):
        width, height = value
        self._native.set_size(Vec2(width, height).native)


__all__ = ["SchematicDiagram"]
