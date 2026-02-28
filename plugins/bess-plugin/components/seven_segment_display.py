from bessplug.api.common.time import TimeNS
from bessplug.api.scene import DrawHookOnDrawResult, SimCompDrawHook
from bessplug.api.sim_engine import (
    ComponentDefinition,
    ComponentState,
    LogicState,
    PinState,
    SlotsGroupInfo,
)

from bessplug.api.common import vec2, vec3, vec4
from bessplug.api.asset_manager import AssetManager
from bessplug.api.scene.renderer import QuadRenderProperties, SubTexture


class SevenSegmentDisplayDrawHook(SimCompDrawHook):
    def __init__(self):
        super().__init__()
        self.draw_enabled = True
        self.tex_id = AssetManager.register_texture_asset(
            "assets/images/7-seg-display-tilemap.png"
        )
        self.sub_textures = []
        self.is_first_draw = True
        self.tex_draw_size = vec2(64, -1)
        self.on_draw_res = DrawHookOnDrawResult()
        self.on_draw_res.draw_children = True
        self.on_draw_res.draw_original = True
        self.on_draw_res.size_changed = False

    def cleanup(self):
        self.sub_textures.clear()

    def onDraw(
        self,
        transform,
        pickingId,
        compState: ComponentState,
        materialRenderer,
        pathRenderer,
    ) -> DrawHookOnDrawResult:
        if self.is_first_draw:
            self.is_first_draw = False
            self._create_subtextures()

        pos = transform.position + vec3(20, 0, 0.0001)
        materialRenderer.draw_sub_textured_quad(
            pos,
            self.tex_draw_size,
            vec4(1, 1, 1, 1),
            pickingId.asUint64(),
            self.sub_textures[0],
            QuadRenderProperties(),
        )

        pos.z += 0.0001

        for idx, inp in enumerate(compState.input_states):
            if inp.state == LogicState.HIGH:
                seg_index = idx + 1
                materialRenderer.draw_sub_textured_quad(
                    pos,
                    self.tex_draw_size,
                    vec4(1, 1, 1, 1),
                    pickingId.asUint64(),
                    self.sub_textures[seg_index],
                    QuadRenderProperties(),
                )
        return self.on_draw_res

    def _create_subtextures(self):
        tex = AssetManager.get_texture_asset(self.tex_id)
        margin = 4
        size = vec2(128, 234)
        sub = SubTexture.create(tex, vec2(0, 0), size, margin, vec2(1, 1))
        self.sub_textures.append(sub)
        sub = SubTexture.create(tex, vec2(1, 0), size, margin, vec2(1, 1))
        self.sub_textures.append(sub)
        sub = SubTexture.create(tex, vec2(2, 0), size, margin, vec2(1, 1))
        self.sub_textures.append(sub)
        sub = SubTexture.create(tex, vec2(3, 0), size, margin, vec2(1, 1))
        self.sub_textures.append(sub)
        sub = SubTexture.create(tex, vec2(4, 0), size, margin, vec2(1, 1))
        self.sub_textures.append(sub)
        sub = SubTexture.create(tex, vec2(0, 1), size, margin, vec2(1, 1))
        self.sub_textures.append(sub)
        sub = SubTexture.create(tex, vec2(1, 1), size, margin, vec2(1, 1))
        self.sub_textures.append(sub)
        sub = SubTexture.create(tex, vec2(2, 1), size, margin, vec2(1, 1))
        self.sub_textures.append(sub)
        tex_size = self.sub_textures[0].size
        self.tex_draw_size.y = self.tex_draw_size.x * (tex_size.y / tex_size.x)
        print(
            f"Created {len(self.sub_textures)} subtextures for 7-seg display, draw size: {self.tex_draw_size}"
        )


def _simulate_seven_segment_display(
    inputs: list[PinState], simTime: float, oldState: ComponentState
) -> ComponentState:
    newState = oldState.copy()
    newState.input_states = inputs.copy()
    newState.is_changed = False
    return newState


input_slots = SlotsGroupInfo()
input_slots.count = 7
input_slots.names = ["A", "B", "C", "D", "E", "F", "G"]
output_slots = SlotsGroupInfo()
output_slots.count = 0

seven_seg_disp_def = ComponentDefinition.from_sim_fn(
    name="Seven Segment Display",
    group_name="IO",
    inputs=input_slots,
    outputs=output_slots,
    sim_delay=TimeNS(1),
    sim_function=_simulate_seven_segment_display,
)
seven_seg_disp_def.compute_hash()
seven_seg_disp_draw_hook = {
    seven_seg_disp_def.get_hash(): SevenSegmentDisplayDrawHook()
}

__all__ = ["seven_seg_disp_def", "seven_seg_disp_draw_hook"]
