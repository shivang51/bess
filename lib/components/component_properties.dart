import 'dart:ui';

import './component_type.dart';

class ComponentProperties{
  String id = "";
  String name = "";
  Offset pos = Offset.zero;
  ComponentType type = ComponentType.none;
}

enum ComponentPropertyType{
  name,
  pos,
  digitalState,
  controlPointPosition,
}