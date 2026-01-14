import datetime
from bessplug.api.scene.sim_comp_draw_hook import SimCompDrawHook
from bessplug.api.common.math import Vec2
from bessplug.api.sim_engine import ComponentDefinition, SlotsGroupInfo, OperatorInfo
from bessplug.api.renderer.path import Path
import math
import datetime
from typing import override

from bessplug.plugin import SchematicDiagram


_gates = {
    "BUF": {
        "name": "Buffer Gate",
        "input_pins": ["A"],
        "output_pins": ["Y"],
        "op": "$",
    },
    "AND": {
        "name": "AND Gate",
        "input_pins": ["A", "B"],
        "output_pins": ["Y"],
        "op": "*",
    },
    "OR": {
        "name": "OR Gate",
        "input_pins": ["A", "B"],
        "output_pins": ["Y"],
        "op": "+",
    },
    "NOT": {
        "name": "NOT Gate",
        "input_pins": ["A"],
        "output_pins": ["Y"],
        "op": "!",
    },
    "NAND": {
        "name": "NAND Gate",
        "input_pins": ["A", "B"],
        "output_pins": ["Y"],
        "op": "*",
        "negate_output": True,
    },
    "NOR": {
        "name": "NOR Gate",
        "input_pins": ["A", "B"],
        "output_pins": ["Y"],
        "op": "+",
        "negate_output": True,
    },
    "XOR": {
        "name": "XOR Gate",
        "input_pins": ["A", "B"],
        "output_pins": ["Y"],
        "op": "^",
    },
    "XNOR": {
        "name": "XNOR Gate",
        "input_pins": ["A", "B"],
        "output_pins": ["Y"],
        "op": "^",
        "negate_output": True,
    },
}


def quadratic_circle_path(cx=0.0, cy=0.0, r=50.0):
    m = math.sqrt(2) / 2
    s = math.sqrt(2) - 1

    return f"""
M {cx + r},{cy}
Q {cx + r},{cy + r*s} {cx + r*m},{cy + r*m}
Q {cx + r*s},{cy + r} {cx},{cy + r}
Q {cx - r*s},{cy + r} {cx - r*m},{cy + r*m}
Q {cx - r},{cy + r*s} {cx - r},{cy}
Q {cx - r},{cy - r*s} {cx - r*m},{cy - r*m}
Q {cx - r*s},{cy - r} {cx},{cy - r}
Q {cx + r*s},{cy - r} {cx + r*m},{cy - r*m}
Q {cx + r},{cy - r*s} {cx + r},{cy}
""".strip()


def _init_paths():
    circle = Path.from_svg_str(quadratic_circle_path(cx=4, cy=4, r=4))
    circle.set_bounds(Vec2(8, 8))
    circle.set_lowest_pos(Vec2(92, 46))
    circle.properties.render_fill = True
    circle.properties.is_closed = True

    # AND Gate
    andPath = Path()
    andPath.move_to(0, 0)
    andPath.line_to(70, 0)
    andPath.quad_to(130, 50, 70, 100)
    andPath.line_to(0, 100)
    andPath.properties.is_closed = True
    andPath.properties.render_fill = True
    andPath.calc_set_bounds()
    andPath.set_bounds(Vec2(100, 100))

    andDiagram = SchematicDiagram()
    andDiagram.add_path(andPath)
    andDiagram.show_name = False
    andDiagram.size = (100, 100)

    # NAND Gate
    nandDiagram = SchematicDiagram()
    nandDiagram.show_name = False
    nandPath = Path()
    nandPath.move_to(0, 0)
    nandPath.line_to(62, 0)
    nandPath.quad_to(122, 50, 62, 100)
    nandPath.line_to(0, 100)
    nandPath.calc_set_bounds()
    nandPath.set_bounds(Vec2(92, 100))
    nandPath.set_lowest_pos(Vec2(0, 0))
    nandPath.properties.is_closed = True
    nandPath.properties.render_fill = True
    nandDiagram.add_path(nandPath)
    nandDiagram.add_path(circle.copy())
    nandDiagram.size = (100, 100)

    # OR Gate
    orDiagram = SchematicDiagram()
    orDiagram.show_name = False
    orPath = Path()
    orPath.move_to(0, 0)
    orPath.line_to(70, 0)
    orPath.quad_to(130, 50, 70, 100)
    orPath.line_to(0, 100)
    orPath.quad_to(30, 50, 0, 0)
    orPath.set_bounds(Vec2(100, 100))
    orPath.set_lowest_pos(Vec2(0, 0))
    orPath.properties.is_closed = True
    orPath.properties.render_fill = True
    orDiagram.add_path(orPath)
    orDiagram.size = (100, 100)

    # NOR Gate
    norDiagram = SchematicDiagram()
    norDiagram.show_name = False
    norPath = Path()
    norPath.move_to(0, 0)
    norPath.line_to(62, 0)
    norPath.quad_to(122, 50, 62, 100)
    norPath.line_to(0, 100)
    norPath.quad_to(30, 50, 0, 0)
    norPath.set_bounds(Vec2(92, 100))
    norPath.set_lowest_pos(Vec2(0, 0))
    norPath.properties.is_closed = True
    norPath.properties.render_fill = True
    norDiagram.add_path(norPath)
    norDiagram.add_path(circle.copy())
    norDiagram.size = (100, 100)

    # XOR
    xorDiagram = SchematicDiagram()
    xorDiagram.show_name = False

    # arc
    xorArcPath = Path()
    xorArcPath.move_to(0, 0)
    xorArcPath.quad_to(30, 50, 0, 100)
    xorArcPath.set_bounds(Vec2(20, 100))
    xorArcPath.set_lowest_pos(Vec2(0, 0))

    # similar to or gate path. just shifted
    xorPath = Path()
    xorPath.move_to(0, 0)
    xorPath.line_to(60, 0)
    xorPath.quad_to(120, 50, 60, 100)
    xorPath.line_to(0, 100)
    xorPath.quad_to(30, 50, 0, 0)
    xorPath.set_bounds(Vec2(90, 100))
    xorPath.set_lowest_pos(Vec2(10, 0))
    xorPath.properties.is_closed = True
    xorPath.properties.render_fill = True

    xorDiagram.add_path(xorArcPath.copy())
    xorDiagram.add_path(xorPath)
    xorDiagram.size = (100, 100)

    # XNOR
    xnorDiagram = SchematicDiagram()
    xnorDiagram.show_name = False

    # similar to or gate path. just shifted
    xnorPath = Path()
    xnorPath.move_to(0, 0)
    xnorPath.line_to(52, 0)
    xnorPath.quad_to(112, 50, 52, 100)
    xnorPath.line_to(0, 100)
    xnorPath.quad_to(30, 50, 0, 0)
    xnorPath.set_bounds(Vec2(82, 100))
    xnorPath.set_lowest_pos(Vec2(10, 0))
    xnorPath.properties.is_closed = True
    xnorPath.properties.render_fill = True

    xnorDiagram.add_path(xorArcPath.copy())
    xnorDiagram.add_path(xnorPath)
    xnorDiagram.add_path(circle.copy())
    xnorDiagram.size = (100, 100)

    # NOT
    notDiagram = SchematicDiagram()
    notDiagram.show_name = False
    notPath = Path()
    notPath.move_to(0, 0)
    notPath.line_to(100, 50)
    notPath.line_to(0, 100)
    notPath.set_bounds(Vec2(100, 100))
    notPath.set_lowest_pos(Vec2(0, 0))
    notPath.properties.is_closed = True
    notPath.properties.render_fill = True
    notDiagram.add_path(notPath)
    notDiagram.size = (100, 100)

    return {
        "AND": andDiagram,
        "NAND": nandDiagram,
        "OR": orDiagram,
        "NOR": norDiagram,
        "XOR": xorDiagram,
        "XNOR": xnorDiagram,
        "NOT": notDiagram,
    }


_paths = _init_paths()


class DrawHook(SimCompDrawHook):
    def __init__(self):
        super().__init__()
        self.schematic_draw_enabled = True

    @override
    def onSchematicDraw(self, state, material_renderer, path_renderer):
        print("DummyHook draw called")


digital_gates: list[ComponentDefinition] = []
draw_hooks: dict[int, type] = {}

for gate_key, gate_data in _gates.items():
    input_slots_info: SlotsGroupInfo = SlotsGroupInfo()
    input_slots_info.count = len(gate_data["input_pins"])
    input_slots_info.is_resizeable = True

    output_slots_info: SlotsGroupInfo = SlotsGroupInfo()
    output_slots_info.count = len(gate_data["output_pins"])

    opInfo = OperatorInfo()
    opInfo.operator_symbol = gate_data["op"]
    opInfo.should_negate_output = gate_data.get("negate_output", False)

    def_gate = ComponentDefinition.from_operator(
        name=gate_data["name"],
        groupName="Digital Gates",
        inputs=input_slots_info,
        outputs=output_slots_info,
        sim_delay=datetime.timedelta(microseconds=0.001),
        op_info=opInfo,
    )
    def_gate.negate = gate_data.get("negate_output", False)
    digital_gates.append(def_gate)

    def_gate.compute_hash()
    draw_hooks[def_gate.get_hash()] = DrawHook


__all__ = ["digital_gates", "draw_hooks"]
