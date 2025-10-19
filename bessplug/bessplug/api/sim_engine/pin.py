"""Pin-related wrappers for the simulation engine.

- PinDetails: Re-exported native struct describing a pin's metadata.
- PinState: Thin Python wrapper around the native pin state with a friendly API.
"""

from __future__ import annotations

from bessplug.bindings import _bindings as _b
from bessplug.bindings._bindings.sim_engine import PinState as NativePinState, PinDetails
from .enums import LogicState

_n = _b.sim_engine

class PinState:
    """Pin signal state with timestamp.

    Wraps the native `PinState` and exposes:
    - state: LogicState value
    - last_change_time_ns: int nanoseconds since last transition
    """

    def __init__(self,
                 native: NativePinState | "PinState" | None = None):
        """Create a PinState.

        - state: Initial logic level
        - last_change_time_ns: Transition timestamp in nanoseconds
        - native: Optional native instance to wrap
        """
        if native is None:
            self._native: NativePinState = _n.PinState()
        elif isinstance(native, PinState):
            self._native = native._native
        else:
            self._native = native


    def is_high(self) -> bool:
        """Check if the pin state is HIGH."""
        return self.state == LogicState.HIGH

    def is_low(self) -> bool:
        """Check if the pin state is LOW."""
        return self.state == LogicState.LOW

    def is_high_z(self) -> bool:
        """Check if the pin state is HIGH_Z."""
        return self.state == LogicState.HIGH_Z

    def invert(self) -> None:
        """Invert the pin state."""
        if self.is_high():
            self.state = LogicState.LOW
        elif self.is_low():
            self.state = LogicState.HIGH

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
    def last_change_time_ns(self, ns: float) -> None:
        """Set last-change timestamp in nanoseconds."""
        self._native.last_change_time_ns = int(ns)

    def __repr__(self) -> str:
        """Debug representation."""
        return f"<PinState state={self.state.name}, t_ns={self.last_change_time_ns}>"


__all__ = [
    "PinDetails",
    "PinState",
]


