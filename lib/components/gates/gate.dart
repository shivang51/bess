import 'package:flutter/cupertino.dart';

import './gate_properties.dart';
import "../component.dart";

abstract class Gate extends Component{
  Gate(){
    properties = GateProperties();
  }
}