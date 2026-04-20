import copy
from typing import override
from bessplug.api.common import theme, vec3
from bessplug.api.scene import PickingId, SimulationSceneComponent
from bessplug.api.sim_engine import ComponentDefinition


class ResistorSComp(SimulationSceneComponent):
    def __init__(self):
        super().__init__()

    @override
    def copy(self):
        cloned = copy.deepcopy(self)
        return cloned

    @override
    def get_type_name(self):
        return "ResistorComp"

    @override
    def to_json(self):
        data = super().to_json()
        return data

    @staticmethod
    @SimulationSceneComponent.deser
    def from_json(data):
        comp = ResistorSComp()
        return comp

    @override
    def draw(self, context):
        id = PickingId()
        id.runtime_id = self.runtime_id
        id.info = 0

        self.draw_background(context)
        self.draw_slots(context)
