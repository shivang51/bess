import 'package:flutter/material.dart';

enum SimulationState { running, paused, stopped }

class AppData with ChangeNotifier {
  SimulationState simulationState = SimulationState.stopped;

  void setSimulationState(SimulationState state) {
    if (simulationState == state) return;
    simulationState = state;
    notifyListeners();
  }

  bool isSimulating() {
    return simulationState != SimulationState.stopped;
  }

  void refresh() {
    notifyListeners();
  }
}
