"""
BESS Python bindings
"""
from __future__ import annotations
import typing
from . import api
__all__: list[str] = ['Plugin', 'api']
class Plugin:
    name: str
    version: str
    def __init__(self) -> None:
        ...
    def cleanup(self) -> None:
        ...
    def on_components_reg_load(self) -> list[api.sim_engine.ComponentDefinition]:
        ...
    def on_scene_comp_load(self) -> dict[int, typing.Any]:
        ...
