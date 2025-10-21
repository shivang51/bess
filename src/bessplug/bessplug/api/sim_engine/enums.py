"""Enum re-exports for the simulation engine.

This module exposes the native C++ enums bound via pybind11, providing
Python-level names consistent with the rest of the API.
"""

from __future__ import annotations
from enum import Enum
from bessplug.bindings._bindings.sim_engine import LogicState as _LogicState, PinType as _PinType, ExtendedPinType as _ExtendedPinType

class PinType(Enum):
    INPUT = _PinType.INPUT
    OUTPUT = _PinType.OUTPUT

    @classmethod
    def from_str(cls, name: str):
        return cls[name.upper()]


class ExtendedPinType(Enum):
    NONE = _ExtendedPinType.NONE
    INPUT_CLOCK = _ExtendedPinType.INPUT_CLOCK
    INPUT_CLEAR = _ExtendedPinType.INPUT_CLEAR

    def is_input_related(self) -> bool:
        return self in (self.INPUT_CLOCK, self.INPUT_CLEAR)

    @classmethod
    def from_str(cls, name: str):
        return cls[name.upper()]


class LogicState(Enum):
    LOW = _LogicState.LOW
    HIGH = _LogicState.HIGH
    UNKNOWN = _LogicState.UNKNOWN
    HIGH_Z = _LogicState.HIGH_Z

    def is_known(self) -> bool:
        """Return True if state is not UNKNOWN or HIGH_Z."""
        return self not in (LogicState.UNKNOWN, LogicState.HIGH_Z)

    def is_high(self) -> bool:
        return self == LogicState.HIGH

    def is_low(self) -> bool:
        return self == LogicState.LOW

    @classmethod
    def from_str(cls, s: str):
        """Create LogicState from string (case-insensitive)."""
        name = s.strip().upper()
        if name in cls.__members__:
            return cls[name]
        raise ValueError(f"Invalid LogicState name: {s}")


__all__ = [
    "LogicState",
    "PinType",
    "ExtendedPinType",
]

