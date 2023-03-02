import 'dart:ui';

import './component_type.dart';

class ComponentProperties {
  String id = "";
  String name = "";
  Offset pos = Offset.zero;
  ComponentType type = ComponentType.none;

  Map<String, dynamic> toJson() => {
        'id': id,
        'name': name,
        'pos': {'x': pos.dx, 'y': pos.dy},
        'type': type.index
      };
}

enum ComponentPropertyType {
  name,
  pos,
  digitalState,
  controlPointPosition,
}
