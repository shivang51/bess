import 'package:bess/data/draw_area/draw_area_data.dart';
import 'package:bess/data/draw_area/objects/io/obj_output_button.dart';
import 'package:bess/data/draw_area/objects/pins/obj_input_pin.dart';
import 'package:uuid/uuid.dart';

class CompOutputProbe {
  final DrawAreaData drawAreaData;
  static const Uuid uuid = Uuid();

  CompOutputProbe({
    required this.drawAreaData,
  }) {
    var id = uuid.v4();
    var pinId = uuid.v4();
    drawAreaData.addObject(
      pinId,
      DAOInputPin(
        id: pinId,
        parentId: id,
        name: "",
      ),
    );

    drawAreaData.addObject(
      id,
      DAOOutputProbe(id: id, name: "Output", pinId: pinId),
    );
  }
}
