import 'package:flutter/material.dart';

class MouseData with ChangeNotifier {
  Offset mousePos = Offset.zero;

  void setMousePos(Offset pos) {
    mousePos = pos;
    notifyListeners();
  }
}
