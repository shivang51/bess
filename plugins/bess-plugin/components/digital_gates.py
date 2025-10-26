from bessplug.api.sim_engine import ComponentDefinition


_gates = {
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


digital_gates = []

for gate_key, gate_data in _gates.items():
    def_gate = ComponentDefinition.from_operator(
        name=gate_data["name"],
        category="Py Digital Gates",
        input_count=len(gate_data["input_pins"]),
        output_count=len(gate_data["output_pins"]),
        delay_ns=1,
        op=gate_data["op"],
    )

    digital_gates.append(def_gate)

__all__ = ["digital_gates"]
