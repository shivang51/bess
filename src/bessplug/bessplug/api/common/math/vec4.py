from bessplug.bindings._bindings.common import vec4


class Vec4(vec4):
    def __init__(self, x: float = 0.0, y: float = 0.0, z: float = 0.0, w: float = 0.0):
        super().__init__(x, y, z, w)


__all__ = ["Vec4"]
