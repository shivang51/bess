import math

from bessplug.api.sim_engine import (
    ANALOG_GROUND_NODE,
    AnalogComponentTrait,
    AnalogStampContext,
    ComponentDefinition,
    SlotsGroupType,
)

from .common import SingleTerminalAnalogComponent, TwoTerminalAnalogComponent, make_slots_info


class DCVoltageSourceComponent(TwoTerminalAnalogComponent):
    def __init__(self, voltage: float = 5.0, comp_name: str = "V1"):
        super().__init__(comp_name)
        self._voltage = voltage

    def stamp(self, context: AnalogStampContext) -> None:
        if not self._connected() or self._shorted():
            return

        if not math.isfinite(self._voltage):
            return

        context.add_voltage_source(
            self._terminals[0],
            self._terminals[1],
            self._voltage,
            self.name(),
        )

    def voltage_source_count(self) -> int:
        return 1 if self._connected() and not self._shorted() else 0

    def numeric_value(self) -> float | None:
        return self._voltage

    def set_numeric_value(self, value: float) -> bool:
        if not math.isfinite(value):
            return False
        self._voltage = value
        return True

    def branch_current_name(self) -> str | None:
        return self.name()


class DCCurrentSourceComponent(TwoTerminalAnalogComponent):
    def __init__(self, current_amps: float = 0.001, comp_name: str = "I1"):
        super().__init__(comp_name)
        self._current_amps = current_amps

    def stamp(self, context: AnalogStampContext) -> None:
        if not self._connected() or self._shorted():
            return

        if not math.isfinite(self._current_amps):
            return

        context.add_current_source(
            self._terminals[0],
            self._terminals[1],
            self._current_amps,
        )


class GroundReferenceComponent(SingleTerminalAnalogComponent):
    def __init__(self, comp_name: str = "GND"):
        super().__init__(comp_name)

    def stamp(self, context: AnalogStampContext) -> None:
        if not self._connected():
            return

        if self._terminal == int(ANALOG_GROUND_NODE):
            return

        context.add_voltage_source(self._terminal, int(ANALOG_GROUND_NODE), 0.0)

    def voltage_source_count(self) -> int:
        if not self._connected() or self._terminal == int(ANALOG_GROUND_NODE):
            return 0
        return 1


dc_voltage_source_def = ComponentDefinition()
dc_voltage_source_def.name = "DC Voltage Source"
dc_voltage_source_def.group_name = "Analog"
dc_voltage_source_def.input_slots_info = make_slots_info(SlotsGroupType.INPUT, 1, ["+"])
dc_voltage_source_def.output_slots_info = make_slots_info(SlotsGroupType.OUTPUT, 1, ["-"])
dc_voltage_source_def.set_analog_trait(
    AnalogComponentTrait(
        2,
        ["+", "-"],
        lambda: DCVoltageSourceComponent(5.0, "V1"),
    )
)

dc_current_source_def = ComponentDefinition()
dc_current_source_def.name = "DC Current Source"
dc_current_source_def.group_name = "Analog"
dc_current_source_def.input_slots_info = make_slots_info(SlotsGroupType.INPUT, 1, ["+"])
dc_current_source_def.output_slots_info = make_slots_info(SlotsGroupType.OUTPUT, 1, ["-"])
dc_current_source_def.set_analog_trait(
    AnalogComponentTrait(
        2,
        ["+", "-"],
        lambda: DCCurrentSourceComponent(0.001, "I1"),
    )
)

ground_def = ComponentDefinition()
ground_def.name = "Ground"
ground_def.group_name = "Analog"
ground_def.input_slots_info = make_slots_info(SlotsGroupType.INPUT, 1, ["GND"])
ground_def.output_slots_info = make_slots_info(SlotsGroupType.OUTPUT, 0, [])
ground_def.set_analog_trait(
    AnalogComponentTrait(
        1,
        ["GND"],
        lambda: GroundReferenceComponent("GND"),
    )
)


__all__ = ["dc_voltage_source_def", "dc_current_source_def", "ground_def"]
