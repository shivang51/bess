import 'package:bess/data/draw_area/objects/draw_area_object.dart';
import 'package:bess/data/draw_area/objects/draw_objects.dart';
import 'package:bess/data/draw_area/objects/gates/obj_nand_gate.dart';
import 'package:bess/data/draw_area/objects/pins/obj_input_pin.dart';
import 'package:bess/data/draw_area/objects/pins/obj_output_pin.dart';
import 'package:flutter/material.dart';

import 'objects/gates/obj_gate.dart';

enum DrawElement { none, connection, pin }

enum DrawElementProperty {
  none,
  pos,
  color,
  name,
}

class ConnectionData {
  final String startGateId;
  final String startPinId;

  ConnectionData(
    this.startGateId,
    this.startPinId,
  );
}

class DrawAreaData with ChangeNotifier {
  Map<String, DrawAreaObject> objects = {};
  String selectedItemId = "";
  Offset areaPos = Offset.zero;
  DrawElement drawingElement = DrawElement.none;
  ConnectionData connStartData = ConnectionData("", "");
  Offset scale = const Offset(1, 1);
  Offset focalPoint = Offset.zero;

  void addObject(String id, DrawAreaObject object) {
    objects[id] = object;
    notifyListeners();
  }

  void addObjects(Map<String, DrawAreaObject> objects) {
    this.objects.addEntries(objects.entries);
    notifyListeners();
  }

  void removeItem(String id) {
    var obj = objects[id]!;
    if (obj.type == DrawObject.wire) {
      objects.remove(id);
    } else if (obj.type == DrawObject.nandGate ||
        obj.type == DrawObject.norGate) {
      var obj_ = obj as DrawAreaGate;
      obj_.inputPins.forEach(objects.remove);
      obj_.outputPins.forEach(objects.remove);
      objects.remove(id);
    }

    notifyListeners();
  }

  void setScale(Offset value) {
    scale = value;
  }

  void setFocalPoint(Offset value) {
    focalPoint = value;
  }

  void setSelectedItemId(String id) {
    if (selectedItemId != id) {
      selectedItemId = id;
      notifyListeners();
    }
  }

  void setAreaPos(Offset pos) {
    if (areaPos != pos) {
      areaPos = pos;
      notifyListeners();
    }
  }

  void setDrawingElement(DrawElement element) {
    if (drawingElement != element) {
      drawingElement = element;
    }
  }

  void setConnStartData(String startGateId, String startPinId) {
    connStartData = ConnectionData(startGateId, startPinId);
  }

  void setProperty(
    String itemId,
    DrawObject type,
    DrawElementProperty property,
    dynamic value,
  ) {
    if (type == DrawObject.pinIn) {
      var pin = objects[itemId]! as DAOInputPin;
      if (property == DrawElementProperty.pos) {
        if (pin.pos != value) {
          pin.pos = value;
          notifyListeners();
        }
      }
    } else if (type == DrawObject.pinOut) {
      var pin = objects[itemId]! as DAOOutputPin;
      if (property == DrawElementProperty.pos) {
        if (pin.pos != value) {
          pin.pos = value;
          notifyListeners();
        }
      }
    }
  }
}
