import 'dart:ui';

import 'package:flutter/cupertino.dart';

import 'types.dart';

abstract class DrawAreaObject {
  String name = "";
  String id = "";
  DrawObjectType type = DrawObjectType.none;
  List<String> connectedItems = [];
  Offset? pos;
  DrawAreaObject({
    required this.name,
    required this.id,
    required this.type,
    this.pos,
  });

  void update(BuildContext context, DigitalState state){}
}
