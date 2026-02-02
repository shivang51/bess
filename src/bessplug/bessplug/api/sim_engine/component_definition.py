from __future__ import annotations
from typing import Callable, Optional, override
from bessplug.api.common import time
from bessplug.api.sim_engine.component_state import ComponentState
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

    @override
    def clone(self) -> ComponentDefinition:
        """Create a deep copy of this ComponentDefinition via Python implementation."""
        cloned = ComponentDefinition()
        cloned.name = self.name
        cloned.group_name = self.group_name
        cloned.sim_delay = self.sim_delay
        cloned.input_slots_info = self.input_slots_info
        cloned.output_slots_info = self.output_slots_info
        cloned.behavior_type = self.behavior_type
        cloned.should_auto_reschedule = self.should_auto_reschedule
        cloned.op_info = self.op_info
        cloned.aux_data = self.aux_data
        cloned.io_growth_policy = self.io_growth_policy
        if self.output_expressions is not None and len(self.output_expressions) > 0:
            cloned.output_expressions = (
                self.output_expressions
            )  # its very important to copy them after aux_data
        cloned.set_simulation_function(self.simulation_function)
        return cloned

    @override
    def get_reschedule_time(self, current_time_ns: time.TimeNS) -> time.TimeNS:
        return current_time_ns

    @override
    def on_state_change(
        self,
        old_state: ComponentState,
        new_state: ComponentState,
    ) -> None:
        pass

    def set_simulation_function(self, sim_function: Callable) -> None:
        self.simulation_function = sim_function

    @staticmethod
    def from_operator(
        name: str,
        group_name: str,
        inputs: SlotsGroupInfo,
        outputs: SlotsGroupInfo,
        sim_delay: time.TimeNS,
        op_info: OperatorInfo,
    ) -> "ComponentDefinition":
        defi = ComponentDefinition()
        defi.set_simulation_function(expr_sim_function)
        defi.name = name
        defi.group_name = group_name
        defi.sim_delay = sim_delay
        defi.input_slots_info = inputs
        defi.output_slots_info = outputs
        defi.op_info = op_info._native
        return defi

    @staticmethod
    def from_expressions(
        name: str,
        group_name: str,
        inputs: SlotsGroupInfo,
        outputs: SlotsGroupInfo,
        sim_delay: time.TimeNS,
        expressions: list[str],
    ) -> "ComponentDefinition":
        defi = ComponentDefinition()
        defi.set_simulation_function(expr_sim_function)
        defi.name = name
        defi.group_name = group_name
        defi.sim_delay = sim_delay
        defi.input_slots_info = inputs
        defi.output_slots_info = outputs
        defi.output_expressions = expressions
        return defi

    @staticmethod
    def from_sim_fn(
        name: str,
        group_name: str,
        inputs: SlotsGroupInfo,
        outputs: SlotsGroupInfo,
        sim_delay: time.TimeNS,
        sim_function: Callable,
    ) -> "ComponentDefinition":
        defi = ComponentDefinition()
        defi.set_simulation_function(sim_function)
        defi.name = name
        defi.group_name = group_name
        defi.sim_delay = sim_delay
        defi.input_slots_info = inputs
        defi.output_slots_info = outputs
        return defi


__all__ = [
    "ComponentDefinition",
]
