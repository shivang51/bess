import 'package:bess/data/draw_area/objects/draw_area_object.dart';
import 'package:bess/data/draw_area/objects/types.dart';

class DAOInputButton extends DrawAreaObject {
  DAOInputButton({
    required super.id,
    required super.name,
    required this.pinId,
    this.state = DigitalState.low,
  }) : super(type: DrawObjectType.inputButton);

  final String pinId;
  final DigitalState state;
}
