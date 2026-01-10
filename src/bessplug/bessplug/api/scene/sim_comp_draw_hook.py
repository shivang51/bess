from bessplug.bindings._bindings.scene import SimCompDrawHook as NativeSimCompDrawHook


class SimCompDrawHook(NativeSimCompDrawHook):
    def __init__(self):
        super().__init__()


__all__ = ["SimCompDrawHook"]
