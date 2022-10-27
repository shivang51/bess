import 'package:bess/data/draw_area/objects/pins/obj_pin.dart';
import 'package:bess/data/draw_area/objects/types.dart';

class DAOOutputPin extends DrawAreaPin {
  DAOOutputPin(
      {required super.id, required super.parentId, required super.name})
      : super(type: DrawObjectType.pinOut);
}
