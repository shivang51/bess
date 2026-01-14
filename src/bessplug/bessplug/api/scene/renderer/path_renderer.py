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


__all__ = ["PathRenderer"]
