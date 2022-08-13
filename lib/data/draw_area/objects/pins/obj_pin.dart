import 'package:bess/data/draw_area/objects/draw_area_object.dart';
import 'package:flutter/cupertino.dart';

abstract class DrawAreaPin extends DrawAreaObject {
  final String parentId;
  Offset? pos;

  DrawAreaPin({
    required this.parentId,
    required super.name,
    required super.id,
    required super.type,
  });
}
