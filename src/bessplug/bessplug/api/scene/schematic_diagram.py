from bessplug.api.common.math import Vec2, Vec3, Vec4
from bessplug.api.scene.renderer import Path, PathRenderer
from bessplug.api.scene.renderer.contours_draw_info import ContoursDrawInfo
from bessplug.bindings._bindings.scene import (
    PickingId,
    SchematicDiagram as NativeSchematicDiagram,
    Transform,
)


class SchematicDiagram(NativeSchematicDiagram):
    """
    Represents a schematic diagram consisting of multiple paths.
    After adding or modifying paths, update the diagram size,
    set it manually using obj.size = (w, h). This size is important for rendering
    paths with correct scale. This should be size of bounding box containing all paths.
    """

    def __init__(self):
        super().__init__()

    def calc_set_size(self):
        """Calculates and sets the size based on the bounds of all paths. Make sure to call this after modifying paths."""

        # FIXME: This is a naive implementation, needs to be fixed to calculate correct bounding box
        w, h = 0, 0
        for path in self.paths:
            bounds = path.get_bounds()
            w += bounds.x
            h += bounds.y
        self.size = Vec2(w, h)

    def scale(self, scale: Vec2):
        """Scales all paths by the given factor."""
        for path in self.paths:
            path = Path.from_native(path)
            path.scale(scale.x, scale.y)

    @staticmethod
    def draw(
        transfrom: Transform,
        pickingId: PickingId,
        path_renderer: PathRenderer,
        diagram: "SchematicDiagram",
    ) -> Vec2:
        pos = transfrom.position
        info = ContoursDrawInfo()

        d_ar = diagram.size.x / diagram.size.y
        t_ar = transfrom.scale.x / transfrom.scale.y
        ad_ar = d_ar / t_ar

        dig_scale = Vec2(transfrom.scale.x, transfrom.scale.y)
        dig_scale.x *= ad_ar

        mid = dig_scale / 2.0

        for path in diagram.paths:
            path.set_stroke_width(diagram.stroke_size)

            path_pos = path.get_lowest_pos()
            path_pos.x *= dig_scale.x
            path_pos.y *= dig_scale.y
            info.translate = Vec3(
                pos.x + path_pos.x - mid.x,
                pos.y + path_pos.y - mid.y,
                pos.z,
            )
            info.scale = dig_scale
            info.gen_stroke = True
            info.glyph_id = pickingId.asUint64()
            info.stroke_color = Vec4(1.0, 1.0, 1.0, 1.0)
            path_renderer.drawPath(path, info)

        return dig_scale


__all__ = ["SchematicDiagram"]
