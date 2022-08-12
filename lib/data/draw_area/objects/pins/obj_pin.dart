import 'package:bess/data/draw_area/objects/draw_area_object.dart';
import 'package:flutter/cupertino.dart';

abstract class DrawAreaPin extends DrawAreaObject {
  final String gateId;
  Offset? pos;

  DrawAreaPin({
    required this.gateId,
    required super.name,
    required super.id,
    required super.type,
  });
}
