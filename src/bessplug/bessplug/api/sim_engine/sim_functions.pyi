"""
Simulation engine prebuilt simulation functions
"""
from __future__ import annotations
import bessplug.api.common.time
import bessplug.api.sim_engine
import collections.abc
__all__: list[str] = ['expr_eval_sim_func']
def expr_eval_sim_func(inputs: collections.abc.Sequence[bessplug.api.sim_engine.PinState], current_time: bessplug.api.common.time.TimeNS, prev_state: bessplug.api.sim_engine.ComponentState) -> bessplug.api.sim_engine.ComponentState:
    """
    Expression evaluator simulation function.
    """
