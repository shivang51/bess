import '../component_properties.dart';

class GateProperties extends ComponentProperties {
  GateProperties() : super();

  List<String> inputPins = [];
  List<String> outputPins = [];

  @override
  Map<String, dynamic> toJson() =>
      {...super.toJson(), 'inputPins': inputPins, 'outputPins': outputPins};
}
