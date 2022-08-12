import 'package:bess/data/draw_area/objects/draw_objects.dart';
import 'package:bess/data/draw_area/objects/pins/obj_pin.dart';

class DAOOutputPin extends DrawAreaPin {
  DAOOutputPin({required super.id, required super.gateId, required super.name})
      : super(type: DrawObject.pinOut);
}
