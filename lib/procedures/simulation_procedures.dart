import 'package:bess/data/draw_area/draw_area_data.dart';
import 'package:bess/data/draw_area/objects/io/obj_input_button.dart';
import 'package:bess/data/app/app_data.dart';
import 'package:bess/data/draw_area/objects/pins/obj_input_pin.dart';
import 'package:bess/data/draw_area/objects/pins/obj_output_pin.dart';
import 'package:bess/data/draw_area/objects/pins/obj_pin.dart';
import 'package:bess/data/draw_area/objects/types.dart';
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
    for(var inputBtn in drawAreaData.inputButtons){
      var pinId = (drawAreaData.objects[inputBtn] as DAOInputButton).pinId;
      var value = (drawAreaData.objects[pinId] as DrawAreaPin).state;
      var connectedItems =  (drawAreaData.objects[pinId] as DrawAreaPin).connectedPins ?? [];
      for (var item in connectedItems) {
        drawAreaData.setProperty(item, DrawObjectType.pinIn, DrawElementProperty.state, value,);
      }
    }
  }

  static void refreshSimulation(BuildContext context, String pinId, {String cPinId = "", bool updateParent=true}){
    var drawAreaData = Provider.of<DrawAreaData>(context, listen: false);
    DigitalState value = (drawAreaData.objects[pinId] as DrawAreaPin).state ?? DigitalState.low;
    var connectedItems =  (drawAreaData.objects[pinId] as DrawAreaPin).connectedPins ?? [];
    if(updateParent) {
      (drawAreaData.objects[pinId] as DrawAreaPin).update(context, value);
    }
    for (var item in connectedItems) {
      drawAreaData.setProperty(item, DrawObjectType.pinIn, DrawElementProperty.state, value,);
      if(item != cPinId) refreshSimulation(context, item, cPinId: pinId);
    }

  }
}
