from typing import override
from bessplug.api.asset_manager import AssetManager, TextureAssetID
from bessplug.api.common import vec2, vec3, vec4
from bessplug.api.scene import PickingId, SimulationSceneComponent
from bessplug.api.scene.renderer import QuadRenderProperties, SubTexture
from bessplug.api.sim_engine import LogicState
import copy


class SevenSegDispComp(SimulationSceneComponent):
    def __init__(self):
        super().__init__()
        self.label_size = 8
        SevenSegDispComp._on_create()

    def __del__(self):
        SevenSegDispComp._on_destroy()

    @override
    def copy(self):
        cloned = copy.deepcopy(self)
        cloned.name = self.name
        cloned.label_size = self.label_size
        return cloned

    @override
    def get_type_name(self):
        return "SevenSegDispComp"

    @override
    def draw(self, context):
        self.draw_background(context)
        self.draw_slots(context)

        transform = self.transform
        pos = transform.position + vec3(20, 0, 0.0001)

        pickingId = PickingId()
        pickingId.runtime_id = self.runtime_id
        pickingId.info = 0

        context.material_renderer.draw_sub_textured_quad(
            pos,
            SevenSegDispComp._tex_draw_size,
            vec4(1, 1, 1, 1),
            pickingId.asUint64(),
            SevenSegDispComp._sub_textures[0],
            QuadRenderProperties(),
        )

        pos.z += 0.0001

        for idx, inp in enumerate(self.get_input_states(context.scene_state)):
            if inp == LogicState.HIGH:
                seg_index = idx + 1
                context.material_renderer.draw_sub_textured_quad(
                    pos,
                    SevenSegDispComp._tex_draw_size,
                    vec4(1, 1, 1, 1),
                    pickingId.asUint64(),
                    SevenSegDispComp._sub_textures[seg_index],
                    QuadRenderProperties(),
                )

    # static members for managin shared texture and subtextures
    _instance_count = 0
    _tex_id = TextureAssetID("")
    _sub_textures = []
    _tex_draw_size = vec2(64, -1)

    @staticmethod
    def _on_create():
        if SevenSegDispComp._instance_count == 0:
            SevenSegDispComp._tex_id = AssetManager.register_texture_asset(
                "assets/images/7-seg-display-tilemap.png"
            )
            SevenSegDispComp._create_subtextures()
        SevenSegDispComp._instance_count += 1

    @staticmethod
    def _on_destroy():
        SevenSegDispComp._instance_count -= 1
        if SevenSegDispComp._instance_count == 0:
            SevenSegDispComp._cleanup_subtextures()

    @staticmethod
    def _create_subtextures():
        tex = AssetManager.get_texture_asset(SevenSegDispComp._tex_id)
        margin = 4
        size = vec2(128, 234)
        sub = SubTexture.create(tex, vec2(0, 0), size, margin, vec2(1, 1))
        SevenSegDispComp._sub_textures.append(sub)
        sub = SubTexture.create(tex, vec2(1, 0), size, margin, vec2(1, 1))
        SevenSegDispComp._sub_textures.append(sub)
        sub = SubTexture.create(tex, vec2(2, 0), size, margin, vec2(1, 1))
        SevenSegDispComp._sub_textures.append(sub)
        sub = SubTexture.create(tex, vec2(3, 0), size, margin, vec2(1, 1))
        SevenSegDispComp._sub_textures.append(sub)
        sub = SubTexture.create(tex, vec2(4, 0), size, margin, vec2(1, 1))
        SevenSegDispComp._sub_textures.append(sub)
        sub = SubTexture.create(tex, vec2(0, 1), size, margin, vec2(1, 1))
        SevenSegDispComp._sub_textures.append(sub)
        sub = SubTexture.create(tex, vec2(1, 1), size, margin, vec2(1, 1))
        SevenSegDispComp._sub_textures.append(sub)
        sub = SubTexture.create(tex, vec2(2, 1), size, margin, vec2(1, 1))
        SevenSegDispComp._sub_textures.append(sub)
        tex_size = SevenSegDispComp._sub_textures[0].size
        SevenSegDispComp._tex_draw_size.y = SevenSegDispComp._tex_draw_size.x * (
            tex_size.y / tex_size.x
        )
        print(
            f"Created {len(SevenSegDispComp._sub_textures)} subtextures for 7-seg display, draw size: {SevenSegDispComp._tex_draw_size}"
        )

    @staticmethod
    def _cleanup_subtextures():
        SevenSegDispComp._sub_textures.clear()
        print("Cleaned up subtextures for 7-seg display")
