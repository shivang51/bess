"""ComponentState wrapper with convenient properties and aux-data helpers."""

from __future__ import annotations


from bessplug.bindings._bindings.sim_engine import (
    ComponentState as NativeComponentState,
)


class ComponentState(NativeComponentState):
    """Python-friendly wrapper for a component's runtime state.

    Exposes input/output pin states, connectivity flags, a change marker,
    and helpers to attach/retrieve arbitrary Python aux data.
    """

    def __init__(self):
        """Create a ComponentState, optionally wrapping a native instance or another wrapper."""
        super().__init__()

    def __str__(self) -> str:
        """JSON representation of the component state."""
        return f'{{"inputs": {self.input_states}, "outputs": {self.output_states}, "input_connected": {self.input_connected}, "output_connected": {self.output_connected}, "changed": {self.changed}}}'

    def __repr__(self) -> str:
        """Debug representation."""
        return f"<ComponentState changed={self.changed} inputs={self.input_states} outputs={self.output_states}>"

    def copy(self) -> "ComponentState":
        """Return a deep copy of this ComponentState."""
        newState = ComponentState()
        newState.aux_data = self.aux_data
        newState.input_states = [ps.copy() for ps in self.input_states]
        newState.output_states = [ps.copy() for ps in self.output_states]
        newState.input_connected = self.input_connected.copy()
        newState.output_connected = self.output_connected.copy()
        newState.changed = self.changed
        return newState


__all__ = [
    "ComponentState",
]
