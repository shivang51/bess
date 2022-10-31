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

  void setTabIndex(int index){
    if(index != tabController.index){
      tabController.index = index;
      notifyListeners();
    }
  }

  void setSimulationState(SimulationState state){
    if(simulationState == state) return;
    simulationState = state;
    notifyListeners();
  }

  bool isSimulating(){
    return simulationState != SimulationState.stopped;
  }

  bool isInSimulationTab(){
    return tabController.index == 1;
  }

  void refresh(){
    notifyListeners();
  }
}
