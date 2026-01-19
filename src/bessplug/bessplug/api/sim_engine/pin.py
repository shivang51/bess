"""Pin-related wrappers for the simulation engine.

- PinDetails: Re-exported native struct describing a pin's metadata.
- PinState: Thin Python wrapper around the native pin state with a friendly API.
"""

from __future__ import annotations

from bessplug.bindings import _bindings as _b
from bessplug.bindings._bindings.sim_engine import PinState as NativePinState
from .enums import LogicState

_n = _b.sim_engine


class PinState(NativePinState):
    """Pin signal state with timestamp.

    Wraps the native `PinState` and exposes:
    - state: LogicState value
    - last_change_time_ns: int nanoseconds since last transition
    """

    def __init__(self, state: LogicState = LogicState.LOW):
        """Create a PinState.

        - state: Initial logic level
        - last_change_time_ns: Transition timestamp in nanoseconds
        - native: Optional native instance to wrap
        """
        super().__init__()
        self.state = state

    def __repr__(self) -> str:
        """Debug representation."""
        return f"<PinState state={self.state.name}, t_ns={self.last_change_time_ns}>"

    def copy(self) -> "PinState":
        """Return a new PinState wrapper copying the native state."""
        state = PinState()
        state.state = self.state
        state.last_change_time_ns = self.last_change_time_ns
        return state


__all__ = [
    "PinState",
]
