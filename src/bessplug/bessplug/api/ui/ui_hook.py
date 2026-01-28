from enum import EnumType
from bessplug.bindings._bindings.ui_hook import (
    UIHook,
    PropertyDescType,
    NumericConstraints,
    EnumConstraints,
    PropertyDesc,
)


def float_constraint(min_value: float, max_value: float, step: float):
    _native = NumericConstraints()
    _native.min = min_value
    _native.max = max_value
    _native.step = step
    return _native


class EnumConstraintsBuilder:
    @staticmethod
    def for_enum(enum_cls: EnumType):
        enum_constraints = EnumConstraints()
        enum_constraints.values = list(
            map(lambda x: x[0], enumerate(enum_cls._member_map_.values()))
        )

        enum_constraints.labels = list(
            map(lambda x: x.name, enum_cls._member_map_.values())
        )

        return enum_constraints


__all__ = [
    "UIHook",
    "PropertyDescType",
    "NumericConstraints",
    "EnumConstraints",
    "PropertyDesc",
    "float_constraint",
    "EnumConstraintsBuilder",
]
