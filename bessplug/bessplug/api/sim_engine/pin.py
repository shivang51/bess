"""Pin-related wrappers for the simulation engine.

- PinDetails: Re-exported native struct describing a pin's metadata.
- PinState: Thin Python wrapper around the native pin state with a friendly API.
"""

from __future__ import annotations

from bessplug.bindings import _bindings as _b
from bessplug.bindings._bindings.sim_engine import PinDetails, PinState
from .enums import LogicState, PinType, ExtendedPinType

_n = _b.sim_engine

class PinState:
    """Pin signal state with timestamp.

    Wraps the native `PinState` and exposes:
    - state: LogicState value
    - last_change_time_ns: int nanoseconds since last transition
    """

    def __init__(self, state: LogicState = LogicState.LOW, last_change_time_ns: int = 0, native: PinDetails = None):
        """Create a PinState.

        - state: Initial logic level
        - last_change_time_ns: Transition timestamp in nanoseconds
        - native: Optional native instance to wrap
        """
        self._native: PinState = native or _n.PinState(state, int(last_change_time_ns))

    @property
    def state(self) -> LogicState:
        """Current logic level for the pin."""
        return self._native.state

    @state.setter
    def state(self, value: LogicState) -> None:
        """Set the pin logic level."""
        self._native.state = value

    @property
    def last_change_time_ns(self) -> int:
        """Nanoseconds since epoch when state last changed."""
        return self._native.last_change_time_ns

    @last_change_time_ns.setter
    def last_change_time_ns(self, ns: int) -> None:
        """Set last-change timestamp in nanoseconds."""
        self._native.last_change_time_ns = int(ns)

    def __repr__(self) -> str:
        """Debug representation."""
        return f"<PinState state={self.state.name}, t_ns={self.last_change_time_ns}>"


__all__ = [
    "PinDetails",
    "PinState",
]


