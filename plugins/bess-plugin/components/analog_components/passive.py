import math

from bessplug.api.sim_engine import (
    AnalogComponentDefinition,
    AnalogComponentState,
    AnalogSolution,
    AnalogStampContext,
)

from .common import TwoTerminalAnalogComponent


class ResistorComponent(TwoTerminalAnalogComponent):
    def __init__(self, resistance_ohms: float = 1000.0, comp_name: str = "R"):
        super().__init__(comp_name)
        self._resistance_ohms = resistance_ohms

    def stamp(self, context: AnalogStampContext) -> None:
        if not self._connected() or self._shorted():
            return

        if not math.isfinite(self._resistance_ohms) or self._resistance_ohms <= 0.0:
            return

        context.add_conductance(
            self._terminals[0],
            self._terminals[1],
            1.0 / self._resistance_ohms,
        )

    def numeric_value(self) -> float | None:
        return self._resistance_ohms

    def set_numeric_value(self, value: float) -> bool:
        if not math.isfinite(value) or value <= 0.0:
            return False

        self._resistance_ohms = value
        return True

    def evaluate_state(self, solution: AnalogSolution) -> AnalogComponentState:
        state = super().evaluate_state(solution)
        if len(state.terminals) != 2:
            return state

        if not state.terminals[0].connected or not state.terminals[1].connected:
            return state

        if (
            self._shorted()
            or not math.isfinite(self._resistance_ohms)
            or self._resistance_ohms <= 0.0
        ):
            return state

        current = (
            state.terminals[0].voltage - state.terminals[1].voltage
        ) / self._resistance_ohms
        state.terminals[0].current = current
        state.terminals[1].current = -current
        return state


resistor_def = AnalogComponentDefinition(
    2,
    [],
    lambda: ResistorComponent(1000.0, "R"),
)
resistor_def.name = "Resistor"
resistor_def.group_name = "Analog"


__all__ = ["resistor_def"]
