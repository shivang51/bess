"""
Scene bindings
"""
from __future__ import annotations
import bessplug.api.common
import bessplug.api.sim_engine
import collections.abc
import typing
from . import renderer
__all__: list[str] = ['DrawHookOnDrawResult', 'PickingId', 'SceneComponent', 'SceneState', 'SchematicDiagram', 'SimCompDrawHook', 'Transform', 'renderer']
class DrawHookOnDrawResult:
    draw_children: bool
    draw_original: bool
    new_size: bessplug.api.common.vec2
    size_changed: bool
    def __init__(self) -> None:
        ...
class PickingId:
    __hash__: typing.ClassVar[None] = None
    @staticmethod
    def invalid() -> PickingId:
        ...
    def __eq__(self, arg0: PickingId) -> bool:
        ...
    def __init__(self) -> None:
        ...
    def asUint64(self) -> int:
        ...
    @property
    def info(self) -> int:
        ...
    @info.setter
    def info(self, arg0: typing.SupportsInt) -> None:
        ...
    @property
    def runtime_id(self) -> int:
        ...
    @runtime_id.setter
    def runtime_id(self, arg0: typing.SupportsInt) -> None:
        ...
class SceneComponent:
    def __init__(self) -> None:
        ...
    def draw(self, arg0: SceneState, arg1: renderer.MaterialRenderer, arg2: renderer.PathRenderer) -> None:
        ...
    def draw_schematic(self, arg0: SceneState, arg1: renderer.MaterialRenderer, arg2: renderer.PathRenderer) -> None:
        ...
class SceneState:
    def __init__(self, arg0: SceneState) -> None:
        ...
    def clear(self) -> None:
        ...
class SchematicDiagram:
    show_name: bool
    size: bessplug.api.common.vec2
    def __init__(self) -> None:
        ...
    def add_path(self, arg0: renderer.Path) -> None:
        ...
    def draw(self, arg0: Transform, arg1: PickingId, arg2: renderer.PathRenderer) -> bessplug.api.common.vec2:
        ...
    def normalize_paths(self) -> None:
        ...
    @property
    def paths(self) -> list[renderer.Path]:
        ...
    @paths.setter
    def paths(self, arg1: collections.abc.Sequence[renderer.Path]) -> None:
        ...
    @property
    def stroke_size(self) -> float:
        ...
    @stroke_size.setter
    def stroke_size(self, arg1: typing.SupportsFloat) -> None:
        ...
class SimCompDrawHook:
    draw_enabled: bool
    schematic_draw_enabled: bool
    def __init__(self) -> None:
        ...
    def cleanup(self) -> None:
        ...
    def onDraw(self, transform: Transform, pickingId: PickingId, compState: bessplug.api.sim_engine.ComponentState, materialRenderer: renderer.MaterialRenderer, pathRenderer: renderer.PathRenderer) -> DrawHookOnDrawResult:
        ...
    def onSchematicDraw(self, transform: Transform, pickingId: PickingId, materialRenderer: renderer.MaterialRenderer, pathRenderer: renderer.PathRenderer) -> bessplug.api.common.vec2:
        ...
class Transform:
    position: bessplug.api.common.vec3
    scale: bessplug.api.common.vec2
    def __init__(self) -> None:
        ...
    @property
    def angle(self) -> float:
        ...
    @angle.setter
    def angle(self, arg0: typing.SupportsFloat) -> None:
        ...
