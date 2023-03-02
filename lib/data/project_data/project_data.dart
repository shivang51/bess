import 'dart:convert';

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

class ProjectData with ChangeNotifier {
  String name = "Untitled";
  String loc = "";
  bool saved = false;
  Map<String, Component> components = {};

  ComponentType drawingComponent = ComponentType.none;

  String selectedItemId = "";
  DrawElement drawingElement = DrawElement.none;
  ConnectionData connStartData = ConnectionData("", "");
  double scaleValue = 1.0;

  Map<ComponentType, List<String>> extraComponents = {
    ComponentType.inputButton: [],
  };

  final List<ComponentType> _extraComponentTypes = [
    ComponentType.inputButton,
  ];

  void _addExtraComponent(String id, Component component) {
    extraComponents[component.properties.type]!.add(id);
  }

  void addComponent(String id, Component component) {
    components[id] = component;
    if (_extraComponentTypes.contains(component.properties.type)) {
      _addExtraComponent(id, component);
    }
    // print(toJson());
    notifyListeners();
  }

  void addComponents(Map<String, Component> components) {
    this.components.addAll(components);
    components.removeWhere(
      (key, value) => !_extraComponentTypes.contains(value.properties.type),
    );
    components.forEach((id, component) {
      _addExtraComponent(id, component);
    });
    notifyListeners();
  }

  void removeComponent(String id) {
    var component = components[id]!;
    if (_extraComponentTypes.contains(component.properties.type)) {
      extraComponents[component.properties.type]!.remove(id);
    }
    components.remove(id);
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

  void updateComponentProperty(
      String id, ComponentPropertyType property, dynamic value) {
    if (!components.containsKey(id)) return;

    var component = components[id]!;

    switch (property) {
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
        (component.properties as WireProperties)
            .controlPointPositions[value["index"]] = value["position"];
        break;
    }

    notifyListeners();
  }

  void setScale(double value) {
    if (scaleValue == value) return;
    scaleValue = value;
    notifyListeners();
  }

  void setDrawingComponent(ComponentType comp) {
    if (comp == drawingComponent) comp = ComponentType.none;
    drawingComponent = comp;
    notifyListeners();
  }

  String toJson() {
    Map<String, dynamic> data = {'name': name, 'components': []};
    components.forEach((key, value) {
      (data['components'] as List<dynamic>).add({key: value.toJson()});
    });
    return json.encode(data);
  }
}
