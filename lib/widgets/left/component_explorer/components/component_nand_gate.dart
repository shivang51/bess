import 'package:bess/data/draw_area/draw_area_data.dart';
import 'package:bess/data/draw_area/objects/gates/obj_nand_gate.dart';
import 'package:bess/data/draw_area/objects/pins/obj_input_pin.dart';
import 'package:bess/data/draw_area/objects/pins/obj_output_pin.dart';
import 'package:uuid/uuid.dart';

class CompNandGate {
  final DrawAreaData drawAreaData;
  static const Uuid uuid = Uuid();

  CompNandGate({
    required this.drawAreaData,
  }) {
    var id = uuid.v4();
    var pinInIds = [uuid.v4(), uuid.v4()];
    var pinOutIds = [uuid.v4()];

    drawAreaData.addObjects({
      pinInIds[0]: DAOInputPin(
        id: pinInIds[0],
        name: "pin",
        gateId: id,
      ),
      pinInIds[1]: DAOInputPin(
        id: pinInIds[1],
        name: "pin",
        gateId: id,
      ),
    });

    drawAreaData.addObjects({
      pinOutIds[0]: DAOOutputPin(
        id: pinOutIds[0],
        name: "pin",
        gateId: id,
      ),
    });

    drawAreaData.addObject(
      id,
      DAONandGate(
        name: "NAND Gate",
        id: id,
        inputPins: pinInIds,
        outputPins: pinOutIds,
      ),
    );
  }
}
