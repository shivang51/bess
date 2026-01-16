from bessplug.bindings._bindings.common import vec2


class Vec2(vec2):
    def __init__(self, x: float = 0.0, y: float = 0.0):
        super().__init__(x, y)


__all__ = ["Vec2"]
