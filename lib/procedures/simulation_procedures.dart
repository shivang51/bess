import 'package:bess/components/component_type.dart';
import 'package:bess/components/components.dart';
import 'package:bess/components/types.dart';
import 'package:bess/data/project_data/project_data.dart';
import 'package:flutter/cupertino.dart';
import 'package:provider/provider.dart';

class SimProcedures {
  static void startSimulation(BuildContext context) {
    ProjectData drawAreaData = Provider.of<ProjectData>(context, listen: false);

    for (String btnId
        in drawAreaData.extraComponents[ComponentType.inputButton]!) {
      InputButton btn = drawAreaData.components[btnId]! as InputButton;
      btn.simulate(context, DigitalState.low, btnId);
    }
  }

  static void stopSimulation(BuildContext context) {}
}
