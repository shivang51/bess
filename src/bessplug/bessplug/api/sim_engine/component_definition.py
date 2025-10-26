from __future__ import annotations
from typing import Callable, Optional, Union, Any
import json

from .pin_detail import PinDetail
from bessplug.bindings._bindings.sim_engine import (
    ComponentDefinition as NativeComponentDefinition,
)
from bessplug.bindings._bindings.sim_engine import (
    SimulationFunction as NativeSimulationFunction,
)

from .sim_engine import expr_sim_function


class ComponentDefinition:
    """
    Provides Pythonic accessors for metadata used by the simulation engine
    and authoring tools:
    - Name, category, timing (delay/setup/hold)
    - Expressions (digital logic), pin details
    - Input/output counts and characteristics
    - Stable content hash for equality/caching
    """

    def __init__(self, native: NativeComponentDefinition = None):
        """Wrap an existing native ComponentDefinition."""
        self._native: NativeComponentDefinition = native or NativeComponentDefinition()
        self._simulate_py: Optional[Callable] = None

    def init(self):
        sim_callable = self._simulate_py or self._native.simulation_function
        if self.op != "0":
            self._native = NativeComponentDefinition(
                self.name,
                self.category,
                int(self.input_count),
                int(self.output_count),
                sim_callable,
                int(self.delay_ns),
                self.op,
            )
        else:
            self._native = NativeComponentDefinition(
                self.name,
                self.category,
                int(self.input_count),
                int(self.output_count),
                sim_callable,
                int(self.delay_ns),
                self.expressions,
            )

    @property
    def simulation_function(self):
        return self._simulate_py or self._native.simulation_function

    @simulation_function.setter
    def simulation_function(
        self, simulate: Union[NativeSimulationFunction, Callable]
    ) -> None:
        self.set_simulation_function(simulate)

    @property
    def name(self) -> str:
        return self._native.name

    @name.setter
    def name(self, v: str) -> None:
        self._native.name = str(v)

    @property
    def category(self) -> str:
        return self._native.category

    @category.setter
    def category(self, v: str) -> None:
        self._native.category = str(v)

    @property
    def delay_ns(self) -> int:
        return self._native.delay_ns

    @delay_ns.setter
    def delay_ns(self, ns: int) -> None:
        self._native.delay_ns = int(ns)

    @property
    def setup_time_ns(self) -> int:
        return self._native.setup_time_ns

    @setup_time_ns.setter
    def setup_time_ns(self, ns: int) -> None:
        self._native.setup_time_ns = int(ns)

    @property
    def hold_time_ns(self) -> int:
        return self._native.hold_time_ns

    @hold_time_ns.setter
    def hold_time_ns(self, ns: int) -> None:
        self._native.hold_time_ns = int(ns)

    @property
    def expressions(self) -> list[str]:
        return list(self._native.expressions)

    @expressions.setter
    def expressions(self, expr: list[str]) -> None:
        self._native.expressions = list(expr)

    @property
    def input_pin_details(self) -> list[PinDetail]:
        return [PinDetail(p) for p in self._native.input_pin_details]

    @property
    def native_input_pin_details(self):
        return self._native.input_pin_details

    @input_pin_details.setter
    def input_pin_details(self, pins: list[PinDetail]) -> None:
        pins = [p._native if isinstance(p, PinDetail) else p for p in pins]
        self._native.input_pin_details = pins

    @property
    def native_output_pin_details(self):
        return self._native.output_pin_details

    @property
    def output_pin_details(self) -> list[PinDetail]:
        return [PinDetail(p) for p in self._native.output_pin_details]

    @output_pin_details.setter
    def output_pin_details(self, pins: list[PinDetail]) -> None:
        pins = [p._native if isinstance(p, PinDetail) else p for p in pins]
        self._native.output_pin_details = pins

    @property
    def input_count(self) -> int:
        return self._native.input_count

    @input_count.setter
    def input_count(self, v: int) -> None:
        self._native.input_count = int(v)

    @property
    def output_count(self) -> int:
        return self._native.output_count

    @output_count.setter
    def output_count(self, v: int) -> None:
        self._native.output_count = int(v)

    @property
    def op(self) -> str:
        return self._native.op

    @op.setter
    def op(self, c: str) -> None:
        self._native.op = (c or "")[0] if c else ""

    @property
    def negate(self) -> bool:
        return self._native.negate

    @negate.setter
    def negate(self, v: bool) -> None:
        self._native.negate = bool(v)

    def set_simulation_function(
        self, simulate: Union[NativeSimulationFunction, Callable]
    ) -> None:
        self._simulate_py = (
            simulate
            if callable(simulate) and not isinstance(simulate, NativeSimulationFunction)
            else None
        )
        sim = (
            simulate
            if isinstance(simulate, NativeSimulationFunction)
            else NativeSimulationFunction(simulate)
        )
        self._native.set_simulation_function(sim)

    def get_expressions(self, input_count: int = -1) -> list[str]:
        return list(self._native.get_expressions(input_count))

    def get_pin_details(self) -> tuple[list[PinDetail], list[PinDetail]]:
        return self._native.get_pin_details()

    def get_hash(self) -> int:
        return int(self._native.get_hash())

    @property
    def aux_data(self):
        return self._native.get_aux_pyobject()

    @aux_data.setter
    def aux_data(self, obj) -> None:
        self._native.set_aux_pyobject(obj)

    def __str__(self) -> str:
        data = {
            "name": self.name,
            "category": self.category,
            "delay_ns": self.delay_ns,
            "setup_time_ns": self.setup_time_ns,
            "hold_time_ns": self.hold_time_ns,
            "expressions": self.expressions,
            "input_pin_details": self.input_pin_details,
            "output_pin_details": self.output_pin_details,
            "input_count": self.input_count,
            "output_count": self.output_count,
            "op": self.op,
            "negate": self.negate,
            "aux_data": self.aux_data,
        }
        return json.dumps(data)

    @staticmethod
    def from_operator(
        name: str,
        category: str,
        input_count: int,
        output_count: int,
        delay_ns: int,
        op: str,
    ) -> "ComponentDefinition":
        simFn = expr_sim_function
        native = NativeComponentDefinition(
            name, category, input_count, output_count, simFn, int(delay_ns), op[0]
        )
        defi = ComponentDefinition(native)
        defi.set_simulation_function(simFn)
        return defi

    @staticmethod
    def from_expressions(
        name: str,
        category: str,
        input_count: int,
        output_count: int,
        delay_ns: int,
        expressions: list[str],
    ) -> "ComponentDefinition":
        simFn = expr_sim_function
        native = NativeComponentDefinition(
            name, category, input_count, output_count, simFn, int(delay_ns), expressions
        )
        defi = ComponentDefinition(native)
        defi.set_simulation_function(simFn)
        return defi

    @staticmethod
    def from_sim_fn(
        name: str,
        category: str,
        input_count: int,
        output_count: int,
        delay_ns: int,
        simFn: NativeSimulationFunction | Callable,
    ) -> "ComponentDefinition":
        native = NativeComponentDefinition(
            name, category, input_count, output_count, simFn, int(delay_ns), []
        )
        defi = ComponentDefinition(native)
        defi.set_simulation_function(simFn)
        return defi


__all__ = [
    "ComponentDefinition",
]
