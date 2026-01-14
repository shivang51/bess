from bessplug.api.common.math.vec3 import Vec3
from bessplug.api.scene import common
from bessplug.api.common.math.vec4 import Vec4
from bessplug.api.scene.common import PickingId
from bessplug.bindings._bindings.scene.renderer import (
    PathRenderer as NativePathRenderer,
)

from .path import Path, PathProperties
from .contours_draw_info import ContoursDrawInfo


class PathRenderer(NativePathRenderer):
    def __init__(self):
        super().__init__()

    def drawPath(self, path: Path, drawInfo: ContoursDrawInfo) -> None:
        super().drawPath(path, drawInfo)

    def beginPath(
        self, startPos: Vec3, weight: float, color: Vec4, id: PickingId
    ) -> Path:
        return super().beginPath(startPos, weight, color, id)

    def endPath(
        self,
        closePath: bool = False,
        genFill: bool = False,
        fillColor: Vec4 = Vec4(1.0, 1.0, 1.0, 1.0),
        genStroke: bool = True,
        roundedJoints: bool = False,
    ) -> None:
        super().endPathMode(closePath, genFill, fillColor, genStroke, roundedJoints)

    def pathMoveTo(self, pos: Vec3) -> None:
        super().pathMoveTo(pos)

    def pathLineTo(self, pos: Vec3, size: float, color: Vec4, id=0) -> None:
        super().pathLineTo(pos, size, color, id)

    def pathQuadTo(
        self,
        controlPoint: Vec3,
        endPoint: Vec3,
        size: float,
        color: Vec4,
        id=0,
    ) -> None:
        super().pathQuadBeizerTo(controlPoint, endPoint, size, color, id)

    def pathCubicTo(
        self,
        controlPoint1: Vec3,
        controlPoint2: Vec3,
        endPoint: Vec3,
        size: float,
        color: Vec4,
        id=0,
    ) -> None:
        super().pathCubicBeizerTo(
            controlPoint1, controlPoint2, endPoint, size, color, id
        )


__all__ = ["PathRenderer"]
