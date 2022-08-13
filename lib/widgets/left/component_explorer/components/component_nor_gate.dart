import 'package:bess/data/draw_area/draw_area_data.dart';
import 'package:bess/data/draw_area/objects/gates/obj_nor_gate.dart';
import 'package:bess/data/draw_area/objects/pins/obj_input_pin.dart';
import 'package:bess/data/draw_area/objects/pins/obj_output_pin.dart';
import 'package:uuid/uuid.dart';

class CompNorGate {
  final DrawAreaData drawAreaData;
  static const Uuid uuid = Uuid();

  CompNorGate({
    required this.drawAreaData,
  }) {
    var id = uuid.v4();
    var pinInIds = [uuid.v4(), uuid.v4()];
    var pinOutIds = [uuid.v4()];

    drawAreaData.addObjects({
      pinInIds[0]: DAOInputPin(
        id: pinInIds[0],
        parentId: id,
        name: "pin",
      ),
      pinInIds[1]: DAOInputPin(
        id: pinInIds[1],
        parentId: id,
        name: "pin",
      ),
    });

    drawAreaData.addObjects({
      pinOutIds[0]: DAOOutputPin(
        id: pinOutIds[0],
        name: "pin",
        parentId: id,
      ),
    });

    drawAreaData.addObject(
      id,
      DAONorGate(
        name: "NOR Gate",
        id: id,
        inputPins: pinInIds,
        outputPins: pinOutIds,
      ),
    );
  }
}
