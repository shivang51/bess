"""
Simulation engine bindings
"""
from __future__ import annotations
import bessplug.api.common.time
import collections.abc
import typing
from . import sim_functions
__all__: list[str] = ['CLEAR', 'CLOCK', 'CompDefIOGrowthPolicy', 'ComponentBehaviorType', 'ComponentDefinition', 'ComponentState', 'ENABLE', 'EQ', 'HIGH', 'HIGH_Z', 'INPUT', 'LOW', 'LogicState', 'NONE', 'OUTPUT', 'OperatorInfo', 'PinState', 'PinType', 'SlotCategory', 'SlotsGroupInfo', 'SlotsGroupType', 'UNKNOWN', 'sim_functions']
class CompDefIOGrowthPolicy:
    """
    Members:
    
      NONE
    
      EQ
    """
    EQ: typing.ClassVar[CompDefIOGrowthPolicy]  # value = <CompDefIOGrowthPolicy.EQ: 1>
    NONE: typing.ClassVar[CompDefIOGrowthPolicy]  # value = <CompDefIOGrowthPolicy.NONE: 0>
    __members__: typing.ClassVar[dict[str, CompDefIOGrowthPolicy]]  # value = {'NONE': <CompDefIOGrowthPolicy.NONE: 0>, 'EQ': <CompDefIOGrowthPolicy.EQ: 1>}
    def __eq__(self, other: typing.Any) -> bool:
        ...
    def __getstate__(self) -> int:
        ...
    def __hash__(self) -> int:
        ...
    def __index__(self) -> int:
        ...
    def __init__(self, value: typing.SupportsInt) -> None:
        ...
    def __int__(self) -> int:
        ...
    def __ne__(self, other: typing.Any) -> bool:
        ...
    def __repr__(self) -> str:
        ...
    def __setstate__(self, state: typing.SupportsInt) -> None:
        ...
    def __str__(self) -> str:
        ...
    @property
    def name(self) -> str:
        ...
    @property
    def value(self) -> int:
        ...
class ComponentBehaviorType:
    """
    Members:
    
      NONE
    
      INPUT
    
      OUTPUT
    """
    INPUT: typing.ClassVar[ComponentBehaviorType]  # value = <ComponentBehaviorType.INPUT: 1>
    NONE: typing.ClassVar[ComponentBehaviorType]  # value = <ComponentBehaviorType.NONE: 0>
    OUTPUT: typing.ClassVar[ComponentBehaviorType]  # value = <ComponentBehaviorType.OUTPUT: 2>
    __members__: typing.ClassVar[dict[str, ComponentBehaviorType]]  # value = {'NONE': <ComponentBehaviorType.NONE: 0>, 'INPUT': <ComponentBehaviorType.INPUT: 1>, 'OUTPUT': <ComponentBehaviorType.OUTPUT: 2>}
    def __eq__(self, other: typing.Any) -> bool:
        ...
    def __getstate__(self) -> int:
        ...
    def __hash__(self) -> int:
        ...
    def __index__(self) -> int:
        ...
    def __init__(self, value: typing.SupportsInt) -> None:
        ...
    def __int__(self) -> int:
        ...
    def __ne__(self, other: typing.Any) -> bool:
        ...
    def __repr__(self) -> str:
        ...
    def __setstate__(self, state: typing.SupportsInt) -> None:
        ...
    def __str__(self) -> str:
        ...
    @property
    def name(self) -> str:
        ...
    @property
    def value(self) -> int:
        ...
class ComponentDefinition:
    behavior_type: ComponentBehaviorType
    group_name: str
    input_slots_info: SlotsGroupInfo
    io_growth_policy: CompDefIOGrowthPolicy
    name: str
    op_info: OperatorInfo
    output_slots_info: SlotsGroupInfo
    should_auto_reschedule: bool
    sim_delay: bessplug.api.common.time.TimeNS
    @staticmethod
    def from_expressions(name: str, group_name: str, inputs: SlotsGroupInfo, outputs: SlotsGroupInfo, sim_delay: bessplug.api.common.time.TimeNS, expressions: collections.abc.Sequence[str]) -> ComponentDefinition:
        """
        Create a ComponentDefinition from output expressions.
        """
    @staticmethod
    def from_operator(name: str, group_name: str, inputs: SlotsGroupInfo, outputs: SlotsGroupInfo, sim_delay: bessplug.api.common.time.TimeNS, op_info: OperatorInfo) -> ComponentDefinition:
        """
        Create a ComponentDefinition from operator info.
        """
    @staticmethod
    def from_sim_fn(name: str, group_name: str, inputs: SlotsGroupInfo, outputs: SlotsGroupInfo, sim_delay: bessplug.api.common.time.TimeNS, sim_function: collections.abc.Callable) -> ComponentDefinition:
        """
        Create a ComponentDefinition from a simulation function.
        """
    def __init__(self) -> None:
        """
        Create an empty, inert component definition.
        """
    def clone(self) -> ComponentDefinition:
        ...
    def compute_hash(self) -> None:
        ...
    def get_hash(self) -> int:
        ...
    def get_reschedule_time(self, current_time_ns: bessplug.api.common.time.TimeNS) -> bessplug.api.common.time.TimeNS:
        """
        Get the next reschedule time given the current time in nanoseconds.
        """
    def on_state_change(self, old_state: ComponentState, new_state: ComponentState) -> None:
        """
        Callback invoked when the component's state changes.
        """
    @property
    def aux_data(self) -> typing.Any:
        """
        Get Set Aux Data as a Python object.
        """
    @aux_data.setter
    def aux_data(self, arg1: typing.Any) -> None:
        ...
    @property
    def output_expressions(self) -> list[str]:
        ...
    @output_expressions.setter
    def output_expressions(self, arg1: collections.abc.Sequence[str]) -> None:
        ...
    @property
    def simulation_function(self) -> typing.Any:
        """
        Get or set the simulation function as a Python callable.
        """
    @simulation_function.setter
    def simulation_function(self, arg1: typing.Any) -> None:
        ...
class ComponentState:
    is_changed: bool
    @typing.overload
    def __init__(self) -> None:
        ...
    @typing.overload
    def __init__(self, arg0: ComponentState) -> None:
        ...
    def clear_aux_data(self) -> None:
        """
        Clear aux_data if it was set via set_aux_pyobject.
        """
    def copy(self) -> ComponentState:
        ...
    def set_output_state(self, idx: typing.SupportsInt, value: PinState) -> None:
        ...
    @property
    def aux_data(self) -> typing.Any:
        """
        Get or set the aux_data as a Python object if it was set via set_aux_pyobject.
        """
    @aux_data.setter
    def aux_data(self, arg1: typing.Any) -> None:
        ...
    @property
    def input_connected(self) -> list[bool]:
        ...
    @input_connected.setter
    def input_connected(self, arg1: collections.abc.Sequence[bool]) -> None:
        ...
    @property
    def input_states(self) -> list[PinState]:
        ...
    @input_states.setter
    def input_states(self, arg1: collections.abc.Sequence[PinState]) -> None:
        ...
    @property
    def output_connected(self) -> list[bool]:
        ...
    @output_connected.setter
    def output_connected(self, arg1: collections.abc.Sequence[bool]) -> None:
        ...
    @property
    def output_states(self) -> list[PinState]:
        ...
    @output_states.setter
    def output_states(self, arg1: collections.abc.Sequence[PinState]) -> None:
        ...
class LogicState:
    """
    Members:
    
      LOW
    
      HIGH
    
      UNKNOWN
    
      HIGH_Z
    """
    HIGH: typing.ClassVar[LogicState]  # value = <LogicState.HIGH: 1>
    HIGH_Z: typing.ClassVar[LogicState]  # value = <LogicState.HIGH_Z: 3>
    LOW: typing.ClassVar[LogicState]  # value = <LogicState.LOW: 0>
    UNKNOWN: typing.ClassVar[LogicState]  # value = <LogicState.UNKNOWN: 2>
    __members__: typing.ClassVar[dict[str, LogicState]]  # value = {'LOW': <LogicState.LOW: 0>, 'HIGH': <LogicState.HIGH: 1>, 'UNKNOWN': <LogicState.UNKNOWN: 2>, 'HIGH_Z': <LogicState.HIGH_Z: 3>}
    def __eq__(self, other: typing.Any) -> bool:
        ...
    def __getstate__(self) -> int:
        ...
    def __hash__(self) -> int:
        ...
    def __index__(self) -> int:
        ...
    def __init__(self, value: typing.SupportsInt) -> None:
        ...
    def __int__(self) -> int:
        ...
    def __ne__(self, other: typing.Any) -> bool:
        ...
    def __repr__(self) -> str:
        ...
    def __setstate__(self, state: typing.SupportsInt) -> None:
        ...
    def __str__(self) -> str:
        ...
    @property
    def name(self) -> str:
        ...
    @property
    def value(self) -> int:
        ...
class OperatorInfo:
    op: str
    should_negate_output: bool
    def __init__(self) -> None:
        ...
class PinState:
    state: LogicState
    @typing.overload
    def __init__(self) -> None:
        ...
    @typing.overload
    def __init__(self, arg0: PinState) -> None:
        ...
    @typing.overload
    def __init__(self, value: bool) -> None:
        ...
    @typing.overload
    def __init__(self, state: LogicState, last_change_time_ns: typing.SupportsInt) -> None:
        ...
    def __repr__(self) -> str:
        ...
    def copy(self) -> PinState:
        ...
    def invert(self) -> None:
        ...
    @property
    def last_change_time_ns(self) -> int:
        ...
    @last_change_time_ns.setter
    def last_change_time_ns(self, arg1: typing.SupportsInt) -> None:
        ...
class PinType:
    """
    Members:
    
      INPUT
    
      OUTPUT
    """
    INPUT: typing.ClassVar[PinType]  # value = <PinType.INPUT: 0>
    OUTPUT: typing.ClassVar[PinType]  # value = <PinType.OUTPUT: 1>
    __members__: typing.ClassVar[dict[str, PinType]]  # value = {'INPUT': <PinType.INPUT: 0>, 'OUTPUT': <PinType.OUTPUT: 1>}
    def __eq__(self, other: typing.Any) -> bool:
        ...
    def __getstate__(self) -> int:
        ...
    def __hash__(self) -> int:
        ...
    def __index__(self) -> int:
        ...
    def __init__(self, value: typing.SupportsInt) -> None:
        ...
    def __int__(self) -> int:
        ...
    def __ne__(self, other: typing.Any) -> bool:
        ...
    def __repr__(self) -> str:
        ...
    def __setstate__(self, state: typing.SupportsInt) -> None:
        ...
    def __str__(self) -> str:
        ...
    @property
    def name(self) -> str:
        ...
    @property
    def value(self) -> int:
        ...
class SlotCategory:
    """
    Members:
    
      NONE
    
      CLOCK
    
      CLEAR
    
      ENABLE
    """
    CLEAR: typing.ClassVar[SlotCategory]  # value = <SlotCategory.CLEAR: 2>
    CLOCK: typing.ClassVar[SlotCategory]  # value = <SlotCategory.CLOCK: 1>
    ENABLE: typing.ClassVar[SlotCategory]  # value = <SlotCategory.ENABLE: 3>
    NONE: typing.ClassVar[SlotCategory]  # value = <SlotCategory.NONE: 0>
    __members__: typing.ClassVar[dict[str, SlotCategory]]  # value = {'NONE': <SlotCategory.NONE: 0>, 'CLOCK': <SlotCategory.CLOCK: 1>, 'CLEAR': <SlotCategory.CLEAR: 2>, 'ENABLE': <SlotCategory.ENABLE: 3>}
    def __eq__(self, other: typing.Any) -> bool:
        ...
    def __getstate__(self) -> int:
        ...
    def __hash__(self) -> int:
        ...
    def __index__(self) -> int:
        ...
    def __init__(self, value: typing.SupportsInt) -> None:
        ...
    def __int__(self) -> int:
        ...
    def __ne__(self, other: typing.Any) -> bool:
        ...
    def __repr__(self) -> str:
        ...
    def __setstate__(self, state: typing.SupportsInt) -> None:
        ...
    def __str__(self) -> str:
        ...
    @property
    def name(self) -> str:
        ...
    @property
    def value(self) -> int:
        ...
class SlotsGroupInfo:
    is_resizeable: bool
    type: SlotsGroupType
    def __init__(self) -> None:
        ...
    @property
    def categories(self) -> list[tuple[int, SlotCategory]]:
        ...
    @categories.setter
    def categories(self, arg0: collections.abc.Sequence[tuple[typing.SupportsInt, SlotCategory]]) -> None:
        ...
    @property
    def count(self) -> int:
        ...
    @count.setter
    def count(self, arg0: typing.SupportsInt) -> None:
        ...
    @property
    def names(self) -> list[str]:
        ...
    @names.setter
    def names(self, arg0: collections.abc.Sequence[str]) -> None:
        ...
class SlotsGroupType:
    """
    Members:
    
      NONE
    
      INPUT
    
      OUTPUT
    """
    INPUT: typing.ClassVar[SlotsGroupType]  # value = <SlotsGroupType.INPUT: 1>
    NONE: typing.ClassVar[SlotsGroupType]  # value = <SlotsGroupType.NONE: 0>
    OUTPUT: typing.ClassVar[SlotsGroupType]  # value = <SlotsGroupType.OUTPUT: 2>
    __members__: typing.ClassVar[dict[str, SlotsGroupType]]  # value = {'NONE': <SlotsGroupType.NONE: 0>, 'INPUT': <SlotsGroupType.INPUT: 1>, 'OUTPUT': <SlotsGroupType.OUTPUT: 2>}
    def __eq__(self, other: typing.Any) -> bool:
        ...
    def __getstate__(self) -> int:
        ...
    def __hash__(self) -> int:
        ...
    def __index__(self) -> int:
        ...
    def __init__(self, value: typing.SupportsInt) -> None:
        ...
    def __int__(self) -> int:
        ...
    def __ne__(self, other: typing.Any) -> bool:
        ...
    def __repr__(self) -> str:
        ...
    def __setstate__(self, state: typing.SupportsInt) -> None:
        ...
    def __str__(self) -> str:
        ...
    @property
    def name(self) -> str:
        ...
    @property
    def value(self) -> int:
        ...
CLEAR: SlotCategory  # value = <SlotCategory.CLEAR: 2>
CLOCK: SlotCategory  # value = <SlotCategory.CLOCK: 1>
ENABLE: SlotCategory  # value = <SlotCategory.ENABLE: 3>
EQ: CompDefIOGrowthPolicy  # value = <CompDefIOGrowthPolicy.EQ: 1>
HIGH: LogicState  # value = <LogicState.HIGH: 1>
HIGH_Z: LogicState  # value = <LogicState.HIGH_Z: 3>
INPUT: ComponentBehaviorType  # value = <ComponentBehaviorType.INPUT: 1>
LOW: LogicState  # value = <LogicState.LOW: 0>
NONE: CompDefIOGrowthPolicy  # value = <CompDefIOGrowthPolicy.NONE: 0>
OUTPUT: ComponentBehaviorType  # value = <ComponentBehaviorType.OUTPUT: 2>
UNKNOWN: LogicState  # value = <LogicState.UNKNOWN: 2>
