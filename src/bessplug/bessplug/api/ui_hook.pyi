"""
UI Hook bindings
"""
from __future__ import annotations
import collections.abc
import typing
__all__: list[str] = ['EnumConstraints', 'NumericConstraints', 'PropertyBinding', 'PropertyDesc', 'PropertyDescType', 'UIHook', 'bool_t', 'enum_t', 'float_t', 'int_t', 'string_t']
class EnumConstraints:
    def __init__(self) -> None:
        ...
    @property
    def labels(self) -> list[str]:
        ...
    @labels.setter
    def labels(self, arg0: collections.abc.Sequence[str]) -> None:
        ...
    @property
    def values(self) -> list[int]:
        ...
    @values.setter
    def values(self, arg0: collections.abc.Sequence[typing.SupportsInt]) -> None:
        ...
class NumericConstraints:
    def __init__(self) -> None:
        ...
    @property
    def max(self) -> float:
        ...
    @max.setter
    def max(self, arg0: typing.SupportsFloat) -> None:
        ...
    @property
    def min(self) -> float:
        ...
    @min.setter
    def min(self, arg0: typing.SupportsFloat) -> None:
        ...
    @property
    def step(self) -> float:
        ...
    @step.setter
    def step(self, arg0: typing.SupportsFloat) -> None:
        ...
class PropertyBinding:
    getter: collections.abc.Callable[[], bool | int | int | float | str | bessplug.api.common.vec4]
    setter: collections.abc.Callable[[bool | typing.SupportsInt | typing.SupportsInt | typing.SupportsFloat | str | bessplug.api.common.vec4], None]
    def __init__(self) -> None:
        ...
class PropertyDesc:
    constraints: None | bessplug.api.ui_hook.NumericConstraints | bessplug.api.ui_hook.EnumConstraints
    name: str
    type: PropertyDescType
    def __init__(self) -> None:
        ...
    @property
    def default_value(self) -> bool | int | int | float | str | bessplug.api.common.vec4:
        ...
    @default_value.setter
    def default_value(self, arg0: bool | typing.SupportsInt | typing.SupportsInt | typing.SupportsFloat | str | bessplug.api.common.vec4) -> None:
        ...
class PropertyDescType:
    """
    Members:
    
      bool_t
    
      int_t
    
      float_t
    
      string_t
    
      enum_t
    """
    __members__: typing.ClassVar[dict[str, PropertyDescType]]  # value = {'bool_t': <PropertyDescType.bool_t: 0>, 'int_t': <PropertyDescType.int_t: 1>, 'float_t': <PropertyDescType.float_t: 3>, 'string_t': <PropertyDescType.string_t: 4>, 'enum_t': <PropertyDescType.enum_t: 5>}
    bool_t: typing.ClassVar[PropertyDescType]  # value = <PropertyDescType.bool_t: 0>
    enum_t: typing.ClassVar[PropertyDescType]  # value = <PropertyDescType.enum_t: 5>
    float_t: typing.ClassVar[PropertyDescType]  # value = <PropertyDescType.float_t: 3>
    int_t: typing.ClassVar[PropertyDescType]  # value = <PropertyDescType.int_t: 1>
    string_t: typing.ClassVar[PropertyDescType]  # value = <PropertyDescType.string_t: 4>
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
class UIHook:
    def __init__(self) -> None:
        ...
    def add_property(self, arg0: PropertyDesc) -> None:
        ...
    def get_properties(self) -> list[PropertyDesc]:
        ...
    def set_properties(self, arg0: collections.abc.Sequence[PropertyDesc]) -> None:
        ...
bool_t: PropertyDescType  # value = <PropertyDescType.bool_t: 0>
enum_t: PropertyDescType  # value = <PropertyDescType.enum_t: 5>
float_t: PropertyDescType  # value = <PropertyDescType.float_t: 3>
int_t: PropertyDescType  # value = <PropertyDescType.int_t: 1>
string_t: PropertyDescType  # value = <PropertyDescType.string_t: 4>
