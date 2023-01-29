import 'component_properties.dart';
import 'types.dart';
import "package:flutter/material.dart";
import 'package:uuid/uuid.dart';

abstract class Component {
  static const Uuid uuid = Uuid();
  ComponentProperties properties = ComponentProperties();
  Widget draw(BuildContext context);
  void simulate(BuildContext context, DigitalState state, String callerId) {}
  void remove(BuildContext context);

  @override
  String toString() {
    return properties.type.name;
  }
}
