from bessplug.bindings._bindings.scene import (
    Transform as NativeTransform,
    PickingId as NativePickingId,
)


class Transform(NativeTransform):
    def __init__(self):
        super().__init__()


class PickingId(NativePickingId):
    def __init__(self):
        super().__init__()


__all__ = ["Transform", "PickingId"]
