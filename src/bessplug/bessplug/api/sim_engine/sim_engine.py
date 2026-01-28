from bessplug.api.sim_engine.component_state import ComponentState
from bessplug.api.sim_engine.pin import PinState
from bessplug.bindings._bindings.sim_engine.sim_functions import expr_eval_sim_func
from bessplug.bindings._bindings.sim_engine import (
    ComponentState as NativeComponentState,
)
import datetime


def expr_sim_function(
    inputs: list[PinState], simTime: float, oldState: NativeComponentState
) -> "ComponentState":
    """A simulation function that evaluates expressions."""
    return expr_eval_sim_func(
        inputs, datetime.timedelta(microseconds=simTime / 1000), oldState
    )


__all__ = ["expr_sim_function"]
