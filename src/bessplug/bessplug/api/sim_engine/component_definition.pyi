from __future__ import annotations
from typing import Callable, Optional, Union, List, Any
from bessplug.api.common import time
from bessplug.api.sim_engine.enums import ComponentBehaviorType
from bessplug.api.sim_engine.operator_info import OperatorInfo
from bessplug.api.sim_engine.slots_group_info import SlotsGroupInfo

from bessplug.bindings._bindings.sim_engine import (
    ComponentDefinition as NativeComponentDefinition,
)
from bessplug.bindings._bindings.sim_engine import (
    SimulationFunction as NativeSimulationFunction,
)

class ComponentDefinition(NativeComponentDefinition):
    """
    Provides Pythonic accessors for metadata used by the simulation engine
    and authoring tools.
    """

    # -----------------
    # Construction
    # -----------------

    def __init__(self, native: NativeComponentDefinition | None = ...) -> None: ...
    def init(self) -> None: ...

    # -----------------
    # Cloning
    # -----------------

    def cloneViaPythonImpl(self) -> ComponentDefinition: ...

    # -----------------
    # Core properties
    # -----------------

    name: str
    group_name: str
    ouput_expressions: List[str]
    sim_delay: time.TimeNS

    should_auto_reschedule: bool
    behavior_type: ComponentBehaviorType

    input_slots_info: SlotsGroupInfo
    output_slots_info: SlotsGroupInfo
    op_info: OperatorInfo

    aux_data: Any

    # -----------------
    # Simulation
    # -----------------

    @property
    def simulation_function(self) -> Optional[Callable[..., Any]]: ...
    @simulation_function.setter
    def simulation_function(
        self,
        simulate: Union[NativeSimulationFunction, Callable[..., Any]],
    ) -> None: ...
    def set_simulation_function(
        self,
        simulate: Union[NativeSimulationFunction, Callable[..., Any]],
    ) -> None: ...

    # -----------------
    # Hash / debug
    # -----------------

    def get_hash(self) -> int: ...
    def __str__(self) -> str: ...

    # -----------------
    # Alt input counts
    # -----------------

    def set_alt_input_counts(self, count: List[int]) -> None: ...
    def get_alt_input_counts(self) -> List[int]: ...

    # -----------------
    # Factories
    # -----------------

    @staticmethod
    def from_operator(
        name: str,
        groupName: str,
        inputs: SlotsGroupInfo,
        outputs: SlotsGroupInfo,
        sim_delay: datetime.timedelta,
        op_info: OperatorInfo,
    ) -> ComponentDefinition: ...
    @staticmethod
    def from_expressions(
        name: str,
        groupName: str,
        inputs: SlotsGroupInfo,
        outputs: SlotsGroupInfo,
        sim_delay: datetime.timedelta,
        expressions: List[str],
    ) -> ComponentDefinition: ...
    @staticmethod
    def from_sim_fn(
        name: str,
        groupName: str,
        inputs: SlotsGroupInfo,
        outputs: SlotsGroupInfo,
        sim_delay: datetime.timedelta,
        simFn: Union[NativeSimulationFunction, Callable[..., Any]],
    ) -> ComponentDefinition: ...

__all__: List[str]
