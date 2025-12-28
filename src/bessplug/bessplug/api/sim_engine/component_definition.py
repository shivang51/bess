from __future__ import annotations
from typing import Any, Callable, Optional, Union, override
import json
import datetime
from bessplug.api.sim_engine.enums import ComponentBehaviorType
from bessplug.api.sim_engine.operator_info import OperatorInfo
from bessplug.api.sim_engine.slots_group_info import SlotsGroupInfo

from bessplug.bindings._bindings.sim_engine import (
    ComponentDefinition as NativeComponentDefinition,
)

from .sim_engine import (
    expr_sim_function,
)


class ComponentDefinition(NativeComponentDefinition):
    """
    Provides Pythonic accessors for metadata used by the simulation engine
    and authoring tools:
    - Name, category, timing (delay/setup/hold)
    - Expressions (digital logic), pin details
    - Input/output counts and characteristics
    - Stable content hash for equality/caching
    """

    def __init__(self):
        super().__init__()
        self.sim_fn_base: Optional[Callable] = None

    def cloneViaPythonImpl(self) -> ComponentDefinition:
        """Create a deep copy of this ComponentDefinition via Python implementation."""
        clone = ComponentDefinition()
        clone.name = self.name
        clone.group_name = self.group_name
        clone.sim_delay = self.sim_delay
        clone.output_expressions = self.output_expressions
        clone.input_slots_info = self.input_slots_info
        clone.output_slots_info = self.output_slots_info
        clone.behavior_type = self.behavior_type
        clone.should_auto_reschedule = self.should_auto_reschedule
        clone.op_info = self.op_info
        clone.aux_data = self.aux_data
        clone.set_simulation_function(self.simulation_function)
        return clone

    def set_simulation_function(self, sim_function: Callable) -> None:
        self.simulation_function = sim_function

    @staticmethod
    def from_operator(
        name: str,
        groupName: str,
        inputs: SlotsGroupInfo,
        outputs: SlotsGroupInfo,
        sim_delay: datetime.timedelta,
        op_info: OperatorInfo,
    ) -> "ComponentDefinition":
        defi = ComponentDefinition()
        defi.set_simulation_function(expr_sim_function)
        defi.name = name
        defi.group_name = groupName
        defi.sim_delay = sim_delay
        defi.input_slots_info = inputs._native
        defi.output_slots_info = outputs._native
        defi.op_info = op_info._native
        return defi

    @staticmethod
    def from_expressions(
        name: str,
        groupName: str,
        inputs: SlotsGroupInfo,
        outputs: SlotsGroupInfo,
        sim_delay: datetime.timedelta,
        expressions: list[str],
    ) -> "ComponentDefinition":
        defi = ComponentDefinition()
        defi.set_simulation_function(expr_sim_function)
        defi.name = name
        defi.group_name = groupName
        defi.sim_delay = sim_delay
        defi.input_slots_info = inputs._native
        defi.output_slots_info = outputs._native
        defi.expressions = expressions
        return defi

    @staticmethod
    def from_sim_fn(
        name: str,
        groupName: str,
        inputs: SlotsGroupInfo,
        outputs: SlotsGroupInfo,
        sim_delay: datetime.timedelta,
        simFn: Callable,
    ) -> "ComponentDefinition":
        defi = ComponentDefinition()
        defi.set_simulation_function(simFn)
        defi.name = name
        defi.group_name = groupName
        defi.sim_delay = sim_delay
        defi.input_slots_info = inputs._native
        defi.output_slots_info = outputs._native
        return defi


__all__ = [
    "ComponentDefinition",
]
