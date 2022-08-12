import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import 'package:bess/data/draw_area/draw_area_data.dart';
import 'package:bess/data/draw_area/objects/gates/obj_gate.dart';
import 'package:bess/data/draw_area/objects/pins/obj_output_pin.dart';
import 'package:bess/data/draw_area/objects/pins/obj_pin.dart';

import 'draw_pin.dart';

class OutputPins extends StatelessWidget {
  const OutputPins({
    Key? key,
    required this.outPins,
    required this.parentId,
    required this.parentPos,
  }) : super(key: key);

  final List<String> outPins;
  final String parentId;
  final Offset parentPos;

  @override
  Widget build(BuildContext context) {
    DrawAreaData drawAreaData =
        Provider.of<DrawAreaData>(context, listen: false);

    return SizedBox(
      height: 100.0,
      width: 20.0,
      child: Column(
        mainAxisAlignment: MainAxisAlignment.spaceAround,
        children: List.generate(
          outPins.length,
          (index) {
            var pin = drawAreaData.objects[outPins[index]] as DAOOutputPin;
            return DrawPin(
              type: pin.type,
              id: outPins[index],
              parentId: parentId,
              parentPos: parentPos + const Offset(110, 49),
            );
          },
        ),
      ),
    );
  }
}
