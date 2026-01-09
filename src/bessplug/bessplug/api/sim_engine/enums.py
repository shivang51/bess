"""Enum re-exports for the simulation engine.

This module exposes the native C++ enums bound via pybind11, providing
Python-level names consistent with the rest of the API.
"""

from __future__ import annotations
from enum import Enum
from bessplug.bindings._bindings.sim_engine import (
    LogicState,
    PinType,
    SlotsGroupType,
    SlotCategory,
    ComponentBehaviorType,
)


__all__ = [
    "LogicState",
    "PinType",
    "SlotsGroupType",
    "SlotCategory",
    "ComponentBehaviorType",
]
