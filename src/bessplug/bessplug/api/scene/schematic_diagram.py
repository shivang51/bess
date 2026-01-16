from bessplug.api.common.math import Vec2, Vec3
from bessplug.api.scene.renderer import Path, PathRenderer
from bessplug.api.scene.renderer.contours_draw_info import ContoursDrawInfo
from bessplug.bindings._bindings.scene import SchematicDiagram as NativeSchematicDiagram


class SchematicDiagram:
    """
    Represents a schematic diagram consisting of multiple paths.
    After adding or modifying paths, update the diagram size,
    set it manually using obj.size = (w, h). This size is important for rendering
    paths with correct scale. This should be size of bounding box containing all paths.
    """

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
        return size.x, size.y

    @size.setter
    def size(self, value: tuple[float, float]):
        width, height = value
        self._native.set_size(Vec2(width, height).native)

    def calc_set_size(self):
        """Calculates and sets the size based on the bounds of all paths. Make sure to call this after modifying paths."""
        w, h = 0, 0
        for path in self.get_paths():
            bounds = path.get_bounds()
            w += bounds.x
            h += bounds.y
        self.size = (w, h)

    @property
    def show_name(self) -> bool:
        return self._native.show_name()

    @show_name.setter
    def show_name(self, value: bool):
        self._native.set_show_name(value)

    @property
    def stroke_size(self) -> float:
        return self._native.get_stroke_size()

    @stroke_size.setter
    def stroke_width(self, value: float):
        self._native.set_stroke_size(value)


def draw_schematic_diagram(
    pos: Vec3, path_renderer: PathRenderer, diagram: SchematicDiagram
):
    for path in diagram.get_paths():
        info = ContoursDrawInfo()
        info.translate = pos
        path_renderer.drawPath(path, info)


__all__ = ["SchematicDiagram"]
