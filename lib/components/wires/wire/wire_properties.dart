import 'dart:ui';

import '../../component_properties.dart';

class WireProperties extends ComponentProperties {
  String startPinId = "";
  String endPinId = "";
  @override
  Map<String, dynamic> toJson() => {
        'startPinId': startPinId,
        'endPinId': endPinId,
      };
}
