import 'package:bess/components/components.dart';
import 'package:bess/data/draw_area/draw_area_data.dart';
import 'package:bess/data/app/app_data.dart';
import 'package:bess/components/types.dart';
import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

class SimProcedures {

  static void startSimulation(BuildContext context) {
    var appData = Provider.of<AppData>(context, listen: false);

    // CHANGE TO SIMULATION TAB
    if(appData.tabController.index != 1) appData.tabController.index = 1;

    // INIT SIMULATION
    initSimulation(context);

    appData.setSimulationState(SimulationState.running );
  }

  static void stopSimulation(BuildContext context) {
    var appData = Provider.of<AppData>(context, listen: false);
    if(appData.tabController.index != 0) appData.tabController.index = 0;
    appData.setSimulationState(SimulationState.stopped);
  }

  static void initSimulation(BuildContext context){
    var drawAreaData = Provider.of<DrawAreaData>(context, listen: false);
    for(var inputBtnId in drawAreaData.inputButtonIds){
      var inputBtn = (drawAreaData.components[inputBtnId] as InputButton);
      inputBtn.simulate(context, DigitalState.low, inputBtnId);
    }
  }
}
