import 'package:bess/data/draw_area/objects/draw_area_object.dart';
import 'package:bess/data/draw_area/objects/pins/obj_input_pin.dart';
import 'package:bess/data/draw_area/objects/pins/obj_output_pin.dart';

abstract class DrawAreaGate extends DrawAreaObject {
  DrawAreaGate({
    required super.id,
    required super.name,
    required super.type,
    required this.inputPins,
    required this.outputPins,
  });

  final List<String> inputPins;
  final List<String> outputPins;
}
