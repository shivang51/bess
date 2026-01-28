from bessplug.bindings._bindings.common import UUID as NativeUUID


class BessUuid(NativeUUID):
    def __init__(self, id: int = -1):
        if id != -1:
            super().__init__(id)
        else:
            super().__init__()

    @staticmethod
    def fromInt(value: int) -> "BessUuid":
        uuid = BessUuid(value)
        return uuid

    def __str__(self) -> str:
        return self.__repr__()
