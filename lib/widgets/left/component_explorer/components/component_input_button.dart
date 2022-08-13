import 'package:bess/data/draw_area/draw_area_data.dart';
import 'package:bess/data/draw_area/objects/io/obj_input_button.dart';
import 'package:bess/data/draw_area/objects/pins/obj_output_pin.dart';
import 'package:uuid/uuid.dart';

class CompInputButton {
  final DrawAreaData drawAreaData;
  static const Uuid uuid = Uuid();

  CompInputButton({
    required this.drawAreaData,
  }) {
    var id = uuid.v4();
    var outputPin = uuid.v4();

    drawAreaData.addObject(
      outputPin,
      DAOOutputPin(
        id: outputPin,
        parentId: id,
        name: "",
      ),
    );

    drawAreaData.addObject(
      id,
      DAOInputButton(id: id, name: "Input", pinId: outputPin),
    );
  }
}
