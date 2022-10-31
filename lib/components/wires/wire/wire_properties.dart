import 'dart:ui';

import '../../component_properties.dart';

class WireProperties extends ComponentProperties{
  String startPinId = "";
  String endPinId = "";
  List<Offset> controlPointPositions = [Offset.zero, Offset.zero];
}