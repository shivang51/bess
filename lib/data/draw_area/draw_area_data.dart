import 'package:bess/components/component.dart';
import 'package:bess/components/component_properties.dart';
import 'package:bess/components/component_type.dart';
import 'package:bess/components/pin/pin_properties.dart';
import 'package:bess/components/wires/wire/wire_properties.dart';
import 'package:flutter/material.dart';

enum DrawElement { none, connection, pin }

enum DrawElementProperty {
  none,
  pos,
  color,
  name,
  controlPoint1,
  controlPoint2,
  addConnection,
  state,
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
  Map<String, Component> components = {};

  String selectedItemId = "";
  DrawElement drawingElement = DrawElement.none;
  ConnectionData connStartData = ConnectionData("", "");

  List<String> inputButtonIds = [];

  void addComponent(String id, Component component) {
    components[id] = component;
    if (component.properties.type == ComponentType.inputButton) {
      inputButtonIds.add(id);
    }
    notifyListeners();
  }

  void addComponents(Map<String, Component> components) {
    this.components.addAll(components);
    components.removeWhere(
      (key, value) => value.properties.type != ComponentType.inputButton,
    );
    inputButtonIds.addAll(components.keys);
    notifyListeners();
  }

  void removeComponent(String id) {
    notifyListeners();
  }

  void setSelectedItemId(String id) {
    if (selectedItemId != id) {
      selectedItemId = id;
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

  void updateComponentProperty(String id,
      ComponentPropertyType property,
      dynamic value
      ){
    if(!components.containsKey(id)) return;

    var component = components[id]!;

    switch(property){
      case ComponentPropertyType.name:
        component.properties.name = value;
        break;
      case ComponentPropertyType.pos:
        component.properties.pos = value;
        break;
      case ComponentPropertyType.digitalState:
        (component.properties as PinProperties).state = value;
        break;
      case ComponentPropertyType.controlPointPosition:
        (component.properties as WireProperties).controlPointPositions[value["index"]] = value["position"];
        break;
    }

    notifyListeners();
  }
}
