import 'package:flutter/material.dart';

enum SimulationState{
  running,
  paused,
  stopped
}


class AppData with ChangeNotifier {
  SimulationState simulationState = SimulationState.stopped;
  late TabController tabController;

  void setTabController(TabController controller){
    tabController = controller;
  }

  void setSimulationState(SimulationState state){
    if(simulationState == state) return;
    simulationState = state;
    notifyListeners();
  }
}
