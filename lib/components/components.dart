library components;

import 'package:bess/components/buttons/button_properties.dart';
import 'package:bess/procedures/simulation_procedures.dart';
import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import 'package:bess/components/component_properties.dart';
import 'package:bess/components/gates/gate_properties.dart';
import 'package:bess/data/app/app_data.dart';
import 'package:bess/data/draw_area/draw_area_data.dart';
import 'package:bess/data/mouse_data.dart';
import 'package:bess/themes.dart';

import 'gates/gate.dart';
import 'gates/nand_gate/nand_painter.dart';

import 'gates/nor_gate/nor_painter.dart';
import 'pin/pin_properties.dart';

import 'component.dart';
import 'component_type.dart';
import 'probes/probe_properties.dart';
import 'types.dart';
import 'wires/wire/control_point_widget.dart';
import 'wires/wire/wire_properties.dart';

part "defaults/defaults.dart";

part 'gates/gate_widget.dart';
part 'gates/nand_gate/nand_gate.dart';
part 'gates/nor_gate/nor_gate.dart';

part 'pin/pin.dart';
part 'pin/pin_widget.dart';
part 'pin/pins_in_widget.dart';
part 'pin/pins_out_widget.dart';

part 'buttons/input_button.dart';
part 'buttons/input_button_widget.dart';

part 'wires/pseudo_wire_widget.dart';
part 'wires/wire/wire.dart';
part 'wires/wire/wire_widget.dart';

part 'probes/output_state_probe.dart';
part 'probes/output_state_probe_widget.dart';
