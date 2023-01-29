import 'package:bess/data/draw_area/draw_area_data.dart';
// import 'package:bess/data/draw_area/objects/io/obj_input_button.dart';
// import 'package:bess/data/simulation/simulation_data.dart';
import 'package:flutter/cupertino.dart';
import 'package:provider/provider.dart';

class SimProcedures {
  static void startSimulation(BuildContext context) {
    DrawAreaData drawAreaData =
        Provider.of<DrawAreaData>(context, listen: false);

    // for (String btnId in drawAreaData.inputButtons) {
    //   DAOInputButton btn = drawAreaData.objects[btnId]! as DAOInputButton;

    // }
  }

  static void stopSimulation(BuildContext context) {}
}
