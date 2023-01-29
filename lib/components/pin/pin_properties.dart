import 'dart:ui';

import 'package:bess/components/components.dart';

import '../component_properties.dart';
import '../types.dart';

class PinProperties extends ComponentProperties{
  String parentId = "";
  DigitalState state = Defaults.pinState;
  PinBehaviour behaviour = PinBehaviour.input;
  double width = 20.0;
  Offset offset = Offset.zero;
  List<String> connectedPinsIds = [];
  Map<String, String> connectedWiresIds = {};
}