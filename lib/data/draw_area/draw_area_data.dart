import 'package:bess/data/draw_area/objects/draw_area_object.dart';
import 'package:bess/data/draw_area/objects/io/obj_input_button.dart';
import 'package:bess/data/draw_area/objects/types.dart';
import 'package:bess/data/draw_area/objects/wires/obj_wire.dart';
import 'package:flutter/material.dart';

import 'objects/gates/obj_gate.dart';
import 'objects/pins/obj_pin.dart';

enum DrawElement { none, connection, pin }

enum DrawElementProperty {
  none,
  pos,
  color,
  name,
  controlPoint1,
  controlPoint2,
  addConnection, state,
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

  List<String> inputButtons = [];

  void addObject(String id, DrawAreaObject object) {
    objects[id] = object;
    if (object.type == DrawObjectType.inputButton) {
      inputButtons.add(id);
    }
    notifyListeners();
  }

  void addObjects(Map<String, DrawAreaObject> objects) {
    this.objects.addEntries(objects.entries);
    notifyListeners();
  }

  void removeItem(String id) {
    var obj = objects[id]!;
    if (obj.type == DrawObjectType.wire) {
      objects.remove(id);
    } else if (obj.type == DrawObjectType.nandGate ||
        obj.type == DrawObjectType.norGate) {
      var obj_ = obj as DrawAreaGate;
      obj_.inputPins.forEach(objects.remove);
      obj_.outputPins.forEach(objects.remove);
      objects.remove(id);
    } else if (obj.type == DrawObjectType.pinIn ||
        obj.type == DrawObjectType.pinOut) {
      var obj_ = obj as DrawAreaPin;
      for (var id_ in obj_.connectedPins) {
        var p = objects[id_]! as DrawAreaPin;
        p.connectedPins.remove(id);
      }
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
    DrawObjectType type,
    DrawElementProperty property,
    dynamic value,
  ) {
    var obj = objects[itemId]!;
    if (property == DrawElementProperty.pos) {
      obj.pos = value;
    } else if (property == DrawElementProperty.controlPoint1) {
      var obj_ = obj as DAOWire;
      obj_.ctrlPoint1 = value;
    } else if (property == DrawElementProperty.controlPoint2) {
      var obj_ = obj as DAOWire;
      obj_.ctrlPoint2 = value;
    } else if (property == DrawElementProperty.addConnection) {
      var obj_ = obj as DrawAreaPin;
      obj_.connectedPins.add(value);
      (objects[value]! as DrawAreaPin).connectedPins.add(itemId);
    }else if(property == DrawElementProperty.state){
      if(type == DrawObjectType.pinIn || type == DrawObjectType.pinOut) {
        (obj as DrawAreaPin).state = value;
      }
    }
    notifyListeners();
  }
}
