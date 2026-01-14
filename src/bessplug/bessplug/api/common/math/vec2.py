from bessplug.bindings._bindings.common import vec2


class Vec2:
    def __init__(self, x: float = 0.0, y: float = 0.0):
        self._native = vec2()
        self._native.x = x
        self._native.y = y

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
    def native(self):
        return self._native

    @native.setter
    def native(self, value: "vec2"):
        self.x = value.x
        self.y = value.y

    @staticmethod
    def from_native(native: "vec2"):
        self = Vec2()
        self._native = native
        return self

    def __iter__(self):
        yield self.x
        yield self.y

    def __getitem__(self, index: int) -> float:
        if index == 0:
            return self.x
        elif index == 1:
            return self.y
        else:
            raise IndexError("Index out of range for Vec2")

    def __setitem__(self, index: int, value: float):
        if index == 0:
            self.x = value
        elif index == 1:
            self.y = value
        else:
            raise IndexError("Index out of range for Vec2")

    def __repr__(self):
        return f"Vec2(x={self.x}, y={self.y})"


__all__ = ["Vec2"]
