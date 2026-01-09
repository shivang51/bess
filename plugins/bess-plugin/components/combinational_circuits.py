from bessplug.api.sim_engine import (
    ComponentDefinition,
    SlotsGroupInfo,
)
import datetime

_comb_circuits = [
    {
        "name": "Full Adder",
        "inputs": ["A", "B", "Cin"],
        "outputs": ["Sum", "Cout"],
        "expressions": ["0^1^2", "(0*1) + 2*(0^1)"],
    },
    {
        "name": "Half Adder",
        "inputs": ["A", "B"],
        "outputs": ["Sum", "Carry"],
        "expressions": ["0^1", "0*1"],
    },
    {
        "name": "2-to-1 Multiplexer",
        "inputs": ["I0", "I1", "S"],
        "outputs": ["Y"],
        "expressions": ["(!2*0) + (2*1)"],
    },
    {
        "name": "1-to-2 Demultiplexer",
        "inputs": ["D", "S"],
        "outputs": ["Y0", "Y1"],
        "expressions": ["0*!1", "0*1"],
    },
    {
        "name": "2-to-4 Decoder",
        "inputs": ["A0", "A1"],
        "outputs": ["D0", "D1", "D2", "D3"],
        "expressions": ["!0*!1", "0*!1", "!0*1", "0*1"],
    },
    {
        "name": "2-bit Magnitude Comparator",
        "inputs": ["A", "B"],
        "outputs": ["A_gt_B", "A_eq_B", "A_lt_B"],
        "expressions": ["0*!1", "!(0^1)", "!0*1"],
    },
    {
        "name": "4-to-1 Multiplexer",
        "inputs": ["I0", "I1", "I2", "I3", "S0", "S1"],
        "outputs": ["Y"],
        "expressions": ["(0*!4*!5) + (1*4*!5) + (2*!4*5) + (3*4*5)"],
    },
    {
        "name": "1-to-4 Demultiplexer",
        "inputs": ["D", "S0", "S1"],
        "outputs": ["Y0", "Y1", "Y2", "Y3"],
        "expressions": ["0*!1*!2", "0*1*!2", "0*!1*2", "0*1*2"],
    },
    {
        "name": "Half Subtractor",
        "inputs": ["A", "B"],
        "outputs": ["Diff", "Bout"],
        "expressions": ["0^1", "!0*1"],
    },
    {
        "name": "Full Subtractor",
        "inputs": ["A", "B", "Bin"],
        "outputs": ["Diff", "Bout"],
        "expressions": ["0^1^2", "(!0*1) + (!(0^1)*2)"],
    },
    {
        "name": "2-bit Priority Encoder",
        "inputs": ["D0", "D1", "D2", "D3"],
        "outputs": ["X", "Y", "V"],
        "expressions": ["2+3", "3+(1*!2)", "0+1+2+3"],
    },
    {
        "name": "BCD to Excess-3 Converter",
        "inputs": ["A", "B", "C", "D"],
        "outputs": ["W", "X", "Y", "Z"],
        "expressions": ["0+(1*2)+(1*3)", "(!1*2)+(!1*3)+(1*!2*!3)", "!(2^3)", "!3"],
    },
    {
        "name": "4-bit Magnitude Comparator (Equal Only)",
        "inputs": ["A3", "A2", "A1", "A0", "B3", "B2", "B1", "B0"],
        "outputs": ["A_eq_B"],
        "expressions": ["!(0^4) * !(1^5) * !(2^6) * !(3^7)"],
    },
    {
        "name": "Gray to Binary Code Converter (3-bit)",
        "inputs": ["G2", "G1", "G0"],
        "outputs": ["B2", "B1", "B0"],
        "expressions": ["0", "0^1", "0^1^2"],
    },
    {
        "name": "2-bit Multiplier",
        "inputs": ["A1", "A0", "B1", "B0"],
        "outputs": ["(MSB) C3", "C2", "C1", "C0"],
        "expressions": [
            "(0*1*2*3)",  # C3 (MSB)
            "(0*2*!(1*3)) + (!(0*2)*1*3)",  # C2
            "(0*3)^(1*2)",  # C1
            "(1*3)",  # C0 (LSB)
        ],
    },
    {
        "name": "Majority Gate (3-input)",
        "inputs": ["A", "B", "C"],
        "outputs": ["Y"],
        "expressions": ["(0*1) + (1*2) + (0*2)"],
    },
    {
        "name": "BCD to Seven Segment Decoder (Segment a)",
        "inputs": ["A", "B", "C", "D"],
        "outputs": ["Sa"],
        "expressions": ["0 + 2 + (1*3) + (!1*!3)"],
    },
    {
        "name": "Active-Low 2-to-4 Decoder",
        "inputs": ["A0", "A1"],
        "outputs": ["!D0", "!D1", "!D2", "!D3"],
        "expressions": ["!(!0*!1)", "!(0*!1)", "!(!0*1)", "!(0*1)"],
    },
]


combinational_circuits = []

for circuit in _comb_circuits:
    inp_grp_info = SlotsGroupInfo()
    inp_grp_info.count = len(circuit["inputs"])
    inp_grp_info.names = circuit["inputs"]

    out_grp_info = SlotsGroupInfo()
    out_grp_info.count = len(circuit["outputs"])
    out_grp_info.names = circuit["outputs"]

    defi = ComponentDefinition.from_expressions(
        name=circuit["name"],
        groupName="Comb Circuits",
        inputs=inp_grp_info,
        outputs=out_grp_info,
        sim_delay=datetime.timedelta(microseconds=0.001),
        expressions=circuit["expressions"],
    )
    combinational_circuits.append(defi)


__all__ = ["combinational_circuits"]
