from bessplug.bindings._bindings.scene.renderer import (
    ContoursDrawInfo as NativeContoursDrawInfo,
)


class ContoursDrawInfo(NativeContoursDrawInfo):
    def __init__(self):
        super().__init__()


__all__ = ["ContoursDrawInfo"]
