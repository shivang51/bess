import 'package:bess/data/draw_area/objects/draw_area_object.dart';
import 'package:bess/data/draw_area/objects/draw_objects.dart';

class DAOInputButton extends DrawAreaObject {
  DAOInputButton({required super.id, required super.name, required this.pinId})
      : super(type: DrawObject.inputButton);

  final String pinId;
}
