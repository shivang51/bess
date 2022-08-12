import 'package:bess/data/draw_area/objects/draw_area_object.dart';
import 'package:bess/data/draw_area/objects/draw_objects.dart';

class DAOWire extends DrawAreaObject {
  String startGateId;
  String startPinId;
  String endGateId;
  String endPinId;

  DAOWire({
    required this.startGateId,
    required this.startPinId,
    required this.endGateId,
    required this.endPinId,
    required super.name,
    required super.id,
  }) : super(type: DrawObject.wire);
}
