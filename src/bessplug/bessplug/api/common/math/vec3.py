from bessplug.bindings._bindings.common import vec3


class Vec3:
    def __init__(self, x: float = 0.0, y: float = 0.0, z: float = 0.0):
        self._native = vec3()
        self._native.x = x
        self._native.y = y
        self._native.z = z

    def copy(self) -> "Vec3":
        return Vec3.from_native(self._native.copy())

    @property
    def x(self) -> float:
        return self._native.x

    @x.setter
    def x(self, value: float):
        self._native.x = value

    @property
    def y(self) -> float:
        return self._native.y

    @y.setter
    def y(self, value: float):
        self._native.y = value

    @property
    def z(self) -> float:
        return self._native.z

    @z.setter
    def z(self, value: float):
        self._native.z = value

    @property
    def native(self):
        return self._native

    @native.setter
    def native(self, value: "vec3"):
        self.x = value.x
        self.y = value.y

    @staticmethod
    def from_native(native: "vec3"):
        self = Vec3()
        self._native = native
        return self

    def __iter__(self):
        yield self.x
        yield self.y

    def __getitem__(self, index: int) -> float:
        return self._native.__getitem__(index)

    def __setitem__(self, index: int, value: float):
        self._native.__setitem__(index, value)

    def __repr__(self):
        return f"Vec3(x={self.x}, y={self.y}, z={self.z})"


__all__ = ["Vec3"]
