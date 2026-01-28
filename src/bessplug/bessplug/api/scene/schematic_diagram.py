from bessplug.api.common.math import Vec2, Vec3, Vec4
from bessplug.api.scene.renderer import Path, PathRenderer
from bessplug.api.scene.renderer.contours_draw_info import ContoursDrawInfo
from bessplug.api.common import theme
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
            path_pos = path.get_lowest_pos()

            info.translate = Vec3(
                pos.x + path_pos.x - mid.x,
                pos.y + path_pos.y - mid.y,
                pos.z,
            )

            info.scale = dig_scale
            info.glyph_id = pickingId.asUint64()
            info.stroke_color = theme.schematic.componentStroke
            info.fill_color = theme.schematic.componentFill

            info.gen_fill = path.get_props().render_fill
            info.close_path = path.get_props().is_closed
            info.gen_stroke = path.get_props().render_stroke
            info.rouned_joint = path.get_props().rounded_joints

            path_renderer.drawPath(path, info)

        return dig_scale


__all__ = ["SchematicDiagram"]
