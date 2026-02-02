"""
Scene Renderer bindings
"""
from __future__ import annotations
import bessplug.api.common
import collections.abc
import typing
__all__: list[str] = ['ContoursDrawInfo', 'Cubic', 'CubicTo', 'Line', 'LineTo', 'MaterialRenderer', 'Move', 'MoveTo', 'Path', 'PathCommand', 'PathCommandKind', 'PathPoint', 'PathProperties', 'PathRenderer', 'Quad', 'QuadRenderProperties', 'QuadTo', 'SubTexture', 'VulkanTexture']
class ContoursDrawInfo:
    close_path: bool
    fill_color: bessplug.api.common.vec4
    gen_fill: bool
    gen_stroke: bool
    rouned_joint: bool
    scale: bessplug.api.common.vec2
    stroke_color: bessplug.api.common.vec4
    translate: bessplug.api.common.vec3
    def __init__(self) -> None:
        ...
    @property
    def glyph_id(self) -> int:
        ...
    @glyph_id.setter
    def glyph_id(self, arg0: typing.SupportsInt) -> None:
        ...
class CubicTo:
    c1: bessplug.api.common.vec2
    c2: bessplug.api.common.vec2
    p: bessplug.api.common.vec2
    def __init__(self) -> None:
        ...
class LineTo:
    p: bessplug.api.common.vec2
    def __init__(self) -> None:
        ...
class MaterialRenderer:
    def draw_circle(self, center: bessplug.api.common.vec3, radius: typing.SupportsFloat, color: bessplug.api.common.vec4, id: typing.SupportsInt, inner_radius: typing.SupportsFloat = 0.0) -> None:
        """
        Draw a colored circle on the screen
        """
    def draw_quad(self, pos: bessplug.api.common.vec3, size: bessplug.api.common.vec2, color: bessplug.api.common.vec4, id: typing.SupportsInt, props: QuadRenderProperties) -> None:
        """
        Draw a colored quad on the screen
        """
    def draw_sub_textured_quad(self, pos: bessplug.api.common.vec3, size: bessplug.api.common.vec2, tint: bessplug.api.common.vec4, id: typing.SupportsInt, sub_texture: SubTexture, props: QuadRenderProperties) -> None:
        """
        Draw a textured quad on the screen using a SubTexture
        """
    def draw_text(self, text: str, position: bessplug.api.common.vec3, size: typing.SupportsInt, color: bessplug.api.common.vec4, id: typing.SupportsInt, angle: typing.SupportsFloat = 0.0) -> None:
        """
        Draw text on the screen
        """
    def draw_textured_quad(self, pos: bessplug.api.common.vec3, size: bessplug.api.common.vec2, tint: bessplug.api.common.vec4, id: typing.SupportsInt, texture: VulkanTexture, props: QuadRenderProperties) -> None:
        """
        Draw a textured quad on the screen using a VulkanTexture
        """
    def get_text_render_size(self, text: str, render_size: typing.SupportsFloat) -> bessplug.api.common.vec2:
        """
        Calculate the size of the rendered text
        """
class MoveTo:
    p: bessplug.api.common.vec2
    def __init__(self) -> None:
        ...
class Path:
    properties: PathProperties
    uuid: bessplug.api.common.UUID
    @staticmethod
    def from_svg_str(svg_data: str) -> Path:
        """
        Create a Path from an SVG path data string.
        """
    def __init__(self) -> None:
        ...
    def __repr__(self) -> str:
        ...
    def add_command(self, arg0: PathCommand) -> Path:
        ...
    def copy(self) -> Path:
        ...
    def cubic_to(self, c1x: typing.SupportsFloat, c1y: typing.SupportsFloat, c2x: typing.SupportsFloat, c2y: typing.SupportsFloat, px: typing.SupportsFloat, py: typing.SupportsFloat) -> Path:
        ...
    def cubic_to_vec(self, arg0: bessplug.api.common.vec2, arg1: bessplug.api.common.vec2, arg2: bessplug.api.common.vec2) -> Path:
        ...
    def get_bounds(self) -> bessplug.api.common.vec2:
        ...
    def get_commands(self) -> list[PathCommand]:
        ...
    def get_contours(self) -> list[list[PathPoint]]:
        ...
    def get_lowest_pos(self) -> bessplug.api.common.vec2:
        ...
    def get_props(self) -> PathProperties:
        ...
    def get_props_ref(self) -> PathProperties:
        ...
    def get_stroke_width(self) -> float:
        ...
    def line_to(self, x: typing.SupportsFloat, y: typing.SupportsFloat) -> Path:
        ...
    def line_to_vec(self, arg0: bessplug.api.common.vec2) -> Path:
        ...
    def move_to(self, x: typing.SupportsFloat, y: typing.SupportsFloat) -> Path:
        ...
    def move_to_vec(self, pos: bessplug.api.common.vec2) -> Path:
        ...
    def normalize(self, size: bessplug.api.common.vec2) -> None:
        ...
    def quad_to(self, cx: typing.SupportsFloat, cy: typing.SupportsFloat, px: typing.SupportsFloat, py: typing.SupportsFloat) -> Path:
        ...
    def quad_to_vec(self, arg0: bessplug.api.common.vec2, arg1: bessplug.api.common.vec2) -> Path:
        ...
    def scale(self, factor: bessplug.api.common.vec2, override_original: bool = False) -> bool:
        ...
    def set_bounds(self, arg0: bessplug.api.common.vec2) -> None:
        ...
    def set_commands(self, arg0: collections.abc.Sequence[PathCommand]) -> None:
        ...
    def set_lowest_pos(self, arg0: bessplug.api.common.vec2) -> None:
        ...
    def set_props(self, arg0: PathProperties) -> None:
        ...
    def set_stroke_width(self, arg0: typing.SupportsFloat) -> None:
        ...
class PathCommand:
    cubic: CubicTo
    kind: PathCommandKind
    line: LineTo
    move: MoveTo
    quad: QuadTo
    def __init__(self) -> None:
        ...
    def __repr__(self) -> str:
        ...
    @property
    def id(self) -> int:
        ...
    @id.setter
    def id(self, arg0: typing.SupportsInt) -> None:
        ...
    @property
    def weight(self) -> float:
        ...
    @weight.setter
    def weight(self, arg0: typing.SupportsFloat) -> None:
        ...
    @property
    def z(self) -> float:
        ...
    @z.setter
    def z(self, arg0: typing.SupportsFloat) -> None:
        ...
class PathCommandKind:
    """
    Members:
    
      Move
    
      Line
    
      Quad
    
      Cubic
    """
    Cubic: typing.ClassVar[PathCommandKind]  # value = <PathCommandKind.Cubic: 3>
    Line: typing.ClassVar[PathCommandKind]  # value = <PathCommandKind.Line: 1>
    Move: typing.ClassVar[PathCommandKind]  # value = <PathCommandKind.Move: 0>
    Quad: typing.ClassVar[PathCommandKind]  # value = <PathCommandKind.Quad: 2>
    __members__: typing.ClassVar[dict[str, PathCommandKind]]  # value = {'Move': <PathCommandKind.Move: 0>, 'Line': <PathCommandKind.Line: 1>, 'Quad': <PathCommandKind.Quad: 2>, 'Cubic': <PathCommandKind.Cubic: 3>}
    def __eq__(self, other: typing.Any) -> bool:
        ...
    def __getstate__(self) -> int:
        ...
    def __hash__(self) -> int:
        ...
    def __index__(self) -> int:
        ...
    def __init__(self, value: typing.SupportsInt) -> None:
        ...
    def __int__(self) -> int:
        ...
    def __ne__(self, other: typing.Any) -> bool:
        ...
    def __repr__(self) -> str:
        ...
    def __setstate__(self, state: typing.SupportsInt) -> None:
        ...
    def __str__(self) -> str:
        ...
    @property
    def name(self) -> str:
        ...
    @property
    def value(self) -> int:
        ...
class PathPoint:
    pass
class PathProperties:
    is_closed: bool
    render_fill: bool
    render_stroke: bool
    rounded_joints: bool
    def __init__(self) -> None:
        ...
    def __repr__(self) -> str:
        ...
class PathRenderer:
    def beginPath(self, startPos: bessplug.api.common.vec3, weight: typing.SupportsFloat, color: bessplug.api.common.vec4, id: typing.SupportsInt) -> None:
        ...
    def drawPath(self, path: Path, info: ContoursDrawInfo) -> None:
        ...
    def endPath(self, closePath: bool = False, genFill: bool = False, fillColor: bessplug.api.common.vec4 = ..., genStroke: bool = True, roundedJoints: bool = False) -> None:
        ...
    def pathCubicTo(self, controlPoint1: bessplug.api.common.vec3, controlPoint2: bessplug.api.common.vec2, endPoint: bessplug.api.common.vec2, size: typing.SupportsFloat, color: bessplug.api.common.vec4, id: typing.SupportsInt) -> None:
        ...
    def pathLineTo(self, pos: bessplug.api.common.vec3, size: typing.SupportsFloat, color: bessplug.api.common.vec4, id: typing.SupportsInt) -> None:
        ...
    def pathMoveTo(self, pos: bessplug.api.common.vec3) -> None:
        ...
    def pathQuadTo(self, controlPoint: bessplug.api.common.vec3, endPoint: bessplug.api.common.vec2, size: typing.SupportsFloat, color: bessplug.api.common.vec4, id: typing.SupportsInt) -> None:
        ...
class QuadRenderProperties:
    borderColor: bessplug.api.common.vec4
    borderRadius: bessplug.api.common.vec4
    borderSize: bessplug.api.common.vec4
    hasShadow: bool
    isMica: bool
    def __init__(self) -> None:
        ...
    @property
    def angle(self) -> float:
        ...
    @angle.setter
    def angle(self, arg0: typing.SupportsFloat) -> None:
        ...
class QuadTo:
    c: bessplug.api.common.vec2
    p: bessplug.api.common.vec2
    def __init__(self) -> None:
        ...
class SubTexture:
    @staticmethod
    def create(texture: VulkanTexture, coord: bessplug.api.common.vec2, sprite_size: bessplug.api.common.vec2, margin: typing.SupportsFloat, cell_size: bessplug.api.common.vec2) -> SubTexture:
        """
        Create a SubTexture from a VulkanTexture with margin and cell size
        """
    @property
    def size(self) -> bessplug.api.common.vec2:
        """
        Get the size of the SubTexture
        """
class VulkanTexture:
    pass
Cubic: PathCommandKind  # value = <PathCommandKind.Cubic: 3>
Line: PathCommandKind  # value = <PathCommandKind.Line: 1>
Move: PathCommandKind  # value = <PathCommandKind.Move: 0>
Quad: PathCommandKind  # value = <PathCommandKind.Quad: 2>
