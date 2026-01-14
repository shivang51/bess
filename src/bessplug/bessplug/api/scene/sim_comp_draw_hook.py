from abc import abstractmethod
from bessplug.bindings._bindings.scene import SimCompDrawHook as NativeSimCompDrawHook


class SimCompDrawHook(NativeSimCompDrawHook):
    def __init__(self):
        super().__init__()

        self.draw_enabled: bool = False
        self.schematic_draw_enabled: bool = False

    @abstractmethod
    def onDraw(self, state, material_renderer, path_renderer):
        """Called when the component needs to be drawn in the scene view."""
        pass

    @abstractmethod
    def onSchematicDraw(self, state, material_renderer, path_renderer):
        """Called when the component needs to be drawn in the schematic view."""
        pass


__all__ = ["SimCompDrawHook"]
