from .enums import SlotGroupType
from bessplug.bindings._bindings.sim_engine import (
    SlotsGroupInfo as NativeSlotsGroupInfo,
)


class SlotsGroupInfo:
    """
    Provides Pythonic accessors for slots group metadata used by the simulation engine
    """

    def __init__(self) -> None:
        self._native: NativeSlotsGroupInfo = NativeSlotsGroupInfo()

    @property
    def type(self) -> SlotGroupType:
        return SlotGroupType(self._native.type)

    @type.setter
    def type(self, value: SlotGroupType) -> None:
        self._native.type = value.value

    @property
    def count(self) -> int:
        return self._native.count

    @count.setter
    def count(self, value: int) -> None:
        self._native.count = value

    @property
    def is_resizeable(self) -> bool:
        return self._native.is_resizeable

    @is_resizeable.setter
    def is_resizeable(self, value: bool) -> None:
        self._native.is_resizeable = value

    @property
    def names(self) -> list[str]:
        return list(self._native.names)

    @names.setter
    def names(self, value: list[str]) -> None:
        self._native.names = value

    @property
    def categories(self) -> list[str]:
        return list(self._native.categories)

    @categories.setter
    def categories(self, value: list[str]) -> None:
        self._native.categories = value


__all__ = ["SlotsGroupInfo"]
