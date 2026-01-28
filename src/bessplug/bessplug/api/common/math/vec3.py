from bessplug.bindings._bindings.common import vec3


class Vec3(vec3):
    def __init__(self, x: float = 0.0, y: float = 0.0, z: float = 0.0):
        super().__init__(x, y, z)

    def copy(self) -> "Vec3":
        return Vec3(self.x, self.y, self.z)


__all__ = ["Vec3"]
