import 'package:bess/data/draw_area/objects/draw_objects.dart';
import 'package:bess/data/draw_area/objects/gates/obj_gate.dart';

class DAONandGate extends DrawAreaGate {
  DAONandGate({
    required super.name,
    required super.id,
    required super.inputPins,
    required super.outputPins,
  }) : super(type: DrawObject.nandGate);
}
