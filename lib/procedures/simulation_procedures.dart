import 'package:bess/components/component_type.dart';
import 'package:bess/components/components.dart';
import 'package:bess/components/types.dart';
import 'package:bess/data/draw_area/draw_area_data.dart';
// import 'package:bess/data/draw_area/objects/io/obj_input_button.dart';
// import 'package:bess/data/simulation/simulation_data.dart';
import 'package:flutter/cupertino.dart';
import 'package:provider/provider.dart';

class SimProcedures {
  static void startSimulation(BuildContext context) {
    DrawAreaData drawAreaData =
        Provider.of<DrawAreaData>(context, listen: false);

    for (String btnId
        in drawAreaData.extraComponents[ComponentType.inputButton]!) {
      InputButton btn = drawAreaData.components[btnId]! as InputButton;
      btn.simulate(context, DigitalState.low, btnId);
    }
  }

  static void stopSimulation(BuildContext context) {}
}
