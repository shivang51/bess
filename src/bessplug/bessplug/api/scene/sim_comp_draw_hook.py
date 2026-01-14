from abc import abstractmethod
from .common import PickingId, Transform
from bessplug.bindings._bindings.scene import SimCompDrawHook as NativeSimCompDrawHook


class SimCompDrawHook(NativeSimCompDrawHook):
    def __init__(self):
        super().__init__()

        self.draw_enabled: bool = False
        self.schematic_draw_enabled: bool = False

    @abstractmethod
    def onDraw(
        self,
        transform: Transform,
        pickingId: PickingId,
        material_renderer,
        path_renderer,
    ):
        """Called when the component needs to be drawn in the scene view."""
        pass

    @abstractmethod
    def onSchematicDraw(
        self,
        transform: Transform,
        pickingId: PickingId,
        material_renderer,
        path_renderer,
    ):
        """Called when the component needs to be drawn in the schematic view."""
        pass


__all__ = ["SimCompDrawHook"]
