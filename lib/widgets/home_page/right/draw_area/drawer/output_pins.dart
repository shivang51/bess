import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import 'package:bess/data/draw_area/draw_area_data.dart';
import 'package:bess/data/draw_area/objects/pins/obj_output_pin.dart';

import 'draw_pin.dart';

class OutputPins extends StatelessWidget {
  const OutputPins({
    Key? key,
    required this.outPins,
    required this.parentId,
    required this.parentPos,
    this.width = 20.0,
  }) : super(key: key);

  final List<String> outPins;
  final String parentId;
  final Offset parentPos;
  final double width;

  @override
  Widget build(BuildContext context) {
    DrawAreaData drawAreaData =
        Provider.of<DrawAreaData>(context, listen: false);

    return SizedBox(
      height: 100.0,
      width: width,
      child: Column(
        mainAxisAlignment: MainAxisAlignment.spaceAround,
        children: List.generate(
          outPins.length,
          (index) {
            var pin = drawAreaData.objects[outPins[index]] as DAOOutputPin;
            return DrawPin(
              type: pin.type,
              id: outPins[index],
              pinPos: parentPos + const Offset(150, 49),
              parentId: parentId,
              width: width,
            );
          },
        ),
      ),
    );
  }
}
