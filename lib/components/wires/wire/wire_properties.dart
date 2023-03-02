import 'dart:ui';

import '../../component_properties.dart';

class WireProperties extends ComponentProperties {
  String startPinId = "";
  String endPinId = "";
  List<Offset> controlPointPositions = [Offset.zero, Offset.zero];
  @override
  Map<String, dynamic> toJson() => {
        'startPinId': startPinId,
        'endPinId': endPinId,
        'controlPointPositions': [
          {'x': controlPointPositions[0].dx, 'y': controlPointPositions[0].dy},
          {'x': controlPointPositions[1].dx, 'y': controlPointPositions[1].dy}
        ],
      };
}
