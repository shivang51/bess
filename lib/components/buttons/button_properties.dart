import '../component_properties.dart';

class ButtonProperties extends ComponentProperties {
  String pinId = "";
  @override
  Map<String, dynamic> toJson() => {
        ...super.toJson(),
        'pinId': pinId,
      };
}
