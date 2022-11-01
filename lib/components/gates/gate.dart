import 'package:bess/components/components.dart';
import 'package:flutter/cupertino.dart';

import './gate_properties.dart';
import "../component.dart";

abstract class Gate extends Component{
  Gate(){
    properties = GateProperties();
  }

  Widget drawGate(BuildContext context,
    {
      required String id,
      required ShapeBorder shapeBorder,
}
  ){
    return GateWidget(id: id, gateObj: this, shapeBorder: shapeBorder,);
  }
}