import 'package:bess/data/draw_area/objects/draw_area_object.dart';
import 'package:bess/data/draw_area/objects/draw_objects.dart';
import 'package:bess/data/draw_area/objects/pins/obj_pin.dart';

class DAOInputPin extends DrawAreaPin {
  DAOInputPin({required super.id, required super.gateId, required super.name})
      : super(type: DrawObject.pinIn);
}
