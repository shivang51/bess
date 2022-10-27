import 'package:bess/data/draw_area/draw_area_data.dart';
import 'package:bess/data/draw_area/objects/pins/obj_pin.dart';
import 'package:bess/data/draw_area/objects/types.dart';
import 'package:bess/data/draw_area/objects/gates/obj_gate.dart';
import 'package:bess/procedures/simulation_procedures.dart';
import 'package:flutter/cupertino.dart';
import 'package:provider/provider.dart';

class DAONandGate extends DrawAreaGate {
  DAONandGate({
    required super.name,
    required super.id,
    required super.inputPins,
    required super.outputPins,
  }) : super(type: DrawObjectType.nandGate);

  @override
  void update(BuildContext context, DigitalState state) {
    var drawAreaData = Provider.of<DrawAreaData>(context, listen: false);

    bool? v_;
    for (var pinId in super.inputPins) {
      var v = (drawAreaData.objects[pinId] as DrawAreaPin).state == DigitalState.high;
      if (v_ == null) {
        v_ = v;
      }else{
        v_ = v_ && v;
      }
    }

    v_ = !v_!;
    DigitalState value = v_ ? DigitalState.high : DigitalState.low;

    for(var pinId in super.outputPins){
      drawAreaData.setProperty(pinId, DrawObjectType.pinOut, DrawElementProperty.state, value,);
      SimProcedures.refreshSimulation(context, pinId, updateParent: false);
    }
  }
}
