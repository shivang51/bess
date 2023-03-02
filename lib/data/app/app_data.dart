import 'package:flutter/material.dart';
import 'package:window_size/window_size.dart';

enum SimulationState { running, paused, stopped }

class AppData with ChangeNotifier {
  SimulationState simulationState = SimulationState.running;
  String baseWindowTitle = "BESS";
  String windowTitle = "BESS";

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

  void setEditorTitle(String name) {
    if (windowTitle == name) return;
    windowTitle = name;
    setWindowTitle(windowTitle);
    notifyListeners();
  }
}
