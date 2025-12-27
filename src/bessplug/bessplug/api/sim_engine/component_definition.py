from __future__ import annotations
from typing import Callable, Optional, Union
import json
import datetime
from bessplug.api.sim_engine.enums import ComponentBehaviorType
from bessplug.api.sim_engine.operator_info import OperatorInfo
from bessplug.api.sim_engine.slots_group_info import SlotsGroupInfo

from bessplug.bindings._bindings.sim_engine import (
    ComponentDefinition as NativeComponentDefinition,
)
from bessplug.bindings._bindings.sim_engine import (
    SimulationFunction as NativeSimulationFunction,
)

from .sim_engine import expr_sim_function


class ComponentDefinition:
    """
    Provides Pythonic accessors for metadata used by the simulation engine
    and authoring tools:
    - Name, category, timing (delay/setup/hold)
    - Expressions (digital logic), pin details
    - Input/output counts and characteristics
    - Stable content hash for equality/caching
    """

    def __init__(self, native: NativeComponentDefinition = None):
        """Wrap an existing native ComponentDefinition."""
        self._native: NativeComponentDefinition = native or NativeComponentDefinition()
        self._simulate_py: Optional[Callable] = None

    def init(self):
        self._native = NativeComponentDefinition()
        self._simulate_py = None

    @property
    def simulation_function(self):
        return self._simulate_py or self._native.simulation_function

    @simulation_function.setter
    def simulation_function(
        self, simulate: Union[NativeSimulationFunction, Callable]
    ) -> None:
        self._native.simulation_function = simulate

    @property
    def name(self) -> str:
        return self._native.name

    @name.setter
    def name(self, v: str) -> None:
        self._native.name = str(v)

    @property
    def group_name(self) -> str:
        return self._native.group_name

    @group_name.setter
    def group_name(self, v: str) -> None:
        self._native.group_name = str(v)

    @property
    def expressions(self) -> list[str]:
        return list(self._native.output_expressions)

    @expressions.setter
    def expressions(self, expr: list[str]) -> None:
        self._native.output_expressions = list(expr)

    @property
    def should_auto_reschedule(self) -> bool:
        return self._native.should_auto_reschedule

    @should_auto_reschedule.setter
    def should_auto_reschedule(self, v: bool) -> None:
        self._native.should_auto_reschedule = bool(v)

    @property
    def behavior_type(self) -> ComponentBehaviorType:
        return self._native.behavior_type

    @behavior_type.setter
    def behavior_type(self, v: ComponentBehaviorType) -> None:
        self._native.behavior_type = v

    @property
    def input_slots_info(self) -> SlotsGroupInfo:
        return self._native.input_slots_info

    @input_slots_info.setter
    def input_slots_info(self, v: SlotsGroupInfo) -> None:
        self._native.input_slots_info = v._native

    @property
    def output_slots_info(self) -> SlotsGroupInfo:
        return self._native.output_slots_info

    @output_slots_info.setter
    def output_slots_info(self, v: SlotsGroupInfo) -> None:
        self._native.output_slots_info = v._native

    @property
    def sim_delay(self) -> datetime.timedelta:
        return self._native.sim_delay

    @sim_delay.setter
    def sim_delay(self, v: datetime.timedelta) -> None:
        self._native.sim_delay = v

    @property
    def op_info(self) -> OperatorInfo:
        return self._native.op_info

    @op_info.setter
    def op_info(self, v: OperatorInfo) -> None:
        self._native.op_info = v

    def set_simulation_function(
        self, simulate: Union[NativeSimulationFunction, Callable]
    ) -> None:
        self._simulate_py = (
            simulate
            if callable(simulate) and not isinstance(simulate, NativeSimulationFunction)
            else None
        )
        sim = (
            simulate
            if isinstance(simulate, NativeSimulationFunction)
            else NativeSimulationFunction(simulate)
        )
        self._native.simulation_function = sim

    def get_hash(self) -> int:
        return int(self._native.get_hash())

    @property
    def aux_data(self):
        return self._native.aux_data

    @aux_data.setter
    def aux_data(self, obj) -> None:
        self._native.aux_data = obj

    def __str__(self) -> str:
        data = {
            "name": self.name,
            "category": self.category,
            "sim_delay": self.sim_delay,
            "expressions": self.expressions,
            "input_slots": self.input_slots_info,
            "output_slots": self.output_slots_info,
            "operator_info": self.op_info,
            "aux_data": self.aux_data,
        }
        return json.dumps(data)

    def set_alt_input_counts(self, count: list[int]) -> None:
        self._native.set_alt_input_counts(count)

    def get_alt_input_counts(self) -> list[int]:
        return list(self._native.get_alt_input_counts())

    @staticmethod
    def from_operator(
        name: str,
        category: str,
        input_count: int,
        output_count: int,
        delay_ns: int,
        op: str,
    ) -> "ComponentDefinition":
        simFn = expr_sim_function
        native = NativeComponentDefinition(
            name, category, input_count, output_count, simFn, int(delay_ns), op[0]
        )
        defi = ComponentDefinition(native)
        defi.set_simulation_function(simFn)
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
        simFn = expr_sim_function
        native = NativeComponentDefinition()
        defi = ComponentDefinition(native)
        defi.set_simulation_function(simFn)
        defi.name = name
        defi.group_name = groupName
        defi.sim_delay = sim_delay
        defi.input_slots_info = inputs
        defi.output_slots_info = outputs
        defi.expressions = expressions
        return defi

    @staticmethod
    def from_sim_fn(
        name: str,
        groupName: str,
        inputs: SlotsGroupInfo,
        outputs: SlotsGroupInfo,
        sim_delay: datetime.timedelta,
        simFn: NativeSimulationFunction | Callable,
    ) -> "ComponentDefinition":
        native = NativeComponentDefinition()
        defi = ComponentDefinition(native)
        defi.set_simulation_function(simFn)
        defi.name = name
        defi.group_name = groupName
        defi.sim_delay = sim_delay
        defi.input_slots_info = inputs
        defi.output_slots_info = outputs
        return defi


__all__ = [
    "ComponentDefinition",
]
