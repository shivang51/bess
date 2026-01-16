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


__all__ = ["PathRenderer"]
