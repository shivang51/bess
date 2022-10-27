import 'package:bess/data/draw_area/objects/draw_area_object.dart';
import 'package:bess/data/draw_area/objects/types.dart';

class DAOOutputProbe extends DrawAreaObject {
  DAOOutputProbe({required super.id, required super.name, required this.pinId})
      : super(type: DrawObjectType.outputProbe);

  final String pinId;
}
