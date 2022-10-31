import 'dart:ui';

import '../component_properties.dart';
import '../types.dart';

class PinProperties extends ComponentProperties{
  String parentId = "";
  DigitalState state = DigitalState.low;
  PinBehaviour behaviour = PinBehaviour.input;
  double width = 20.0;
  Offset offset = Offset.zero;
  List<String> connectedPinsIds = [];
}