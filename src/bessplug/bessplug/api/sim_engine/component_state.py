"""ComponentState wrapper with convenient properties and aux-data helpers."""

from __future__ import annotations


from bessplug.bindings import _bindings as _b
from bessplug.bindings._bindings.sim_engine import ComponentState as NativeComponentState
from bessplug.bindings._bindings.sim_engine import PinState

_n = _b.sim_engine

class ComponentState:
    """Python-friendly wrapper for a component's runtime state.

    Exposes input/output pin states, connectivity flags, a change marker,
    and helpers to attach/retrieve arbitrary Python aux data.
    """

    def __init__(self, native: NativeComponentState | "ComponentState" | None = None):
        """Create a ComponentState, optionally wrapping a native instance or another wrapper."""
        if native is None:
            self._native: NativeComponentState = _n.ComponentState()
        elif isinstance(native, ComponentState):
            self._native = native._native
        else:
            self._native = native

    # IO state lists (native objects)
    @property
    def input_states(self):
        """List of input `PinState` (native objects)."""
        return self._native.input_states

    @property
    def output_states(self):
        """List of output `PinState` (native objects)."""
        return self._native.output_states

    @input_states.setter
    def input_states(self, v) -> None:
        """Set input pin states."""
        # Normalize to native list to avoid wrapper copies
        if isinstance(v, list):
            nv = [ps._native if hasattr(ps, "_native") else ps for ps in v]
            self._native.input_states = nv
        else:
            self._native.input_states = v

    @output_states.setter
    def output_states(self, v) -> None:
        """Set output pin states."""
        # Normalize to native list to avoid wrapper copies
        if isinstance(v, list):
            nv = [ps._native if hasattr(ps, "_native") else ps for ps in v]
            self._native.output_states = nv
        else:
            self._native.output_states = v

    def set_output_state(self, idx: int, pin_state: PinState) -> None:
        """Set a specific output pin state by index."""
        ps = pin_state._native if hasattr(pin_state, '_native') else pin_state
        # Prefer native method if present (ensures reference semantics)
        try:
            self._native.set_output_state(idx, ps)
        except Exception:
            self._native.output_states[idx] = ps

    # Connectivity
    @property
    def input_connected(self) -> list[bool]:
        """Boolean connectivity flags per input pin."""
        return self._native.input_connected

    @input_connected.setter
    def input_connected(self, v: list[bool]) -> None:
        """Set connectivity flags for inputs."""
        self._native.input_connected = v

    @property
    def output_connected(self) -> list[bool]:
        """Boolean connectivity flags per output pin."""
        return self._native.output_connected

    @output_connected.setter
    def output_connected(self, v: list[bool]) -> None:
        """Set connectivity flags for outputs."""
        self._native.output_connected = v

    # Change flag
    @property
    def changed(self) -> bool:
        """True if the component's state changed in the last step."""
        return self._native.is_changed

    @changed.setter
    def changed(self, value: bool) -> None:
        """Set the changed flag."""
        self._native.is_changed = bool(value)

    def mark_changed(self, value: bool = True) -> None:
        """Mark the state as changed (or not)."""
        self._native.is_changed = bool(value)

    @property
    def aux_data(self):
        """Get Python aux object attached to this state, or None."""
        return self._native.get_aux_pyobject()

    @aux_data.setter
    def aux_data(self, obj) -> None:
        """Attach a Python aux object to this state."""
        self._native.set_aux_pyobject(obj)

    def __str__(self) -> str:
        """JSON representation of the component state."""
        return f'{{"inputs": {self.input_states}, "outputs": {self.output_states}, "input_connected": {self.input_connected}, "output_connected": {self.output_connected}, "changed": {self.changed}}}'

    def __repr__(self) -> str:
        """Debug representation."""
        return f"<ComponentState changed={self.changed} inputs={self.input_states} outputs={self.output_states}>"

    def copy(self) -> "ComponentState":
        """Return a deep copy of this ComponentState."""
        # Use native copy ctor and re-wrap
        native_copy = _n.ComponentState(self._native)
        return ComponentState(native_copy)


__all__ = [
    "ComponentState",
]


