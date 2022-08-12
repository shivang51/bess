import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import 'package:bess/data/draw_area/draw_area_data.dart';
import 'package:bess/data/draw_area/objects/pins/obj_input_pin.dart';

import 'draw_pin.dart';

class InputPins extends StatelessWidget {
  const InputPins({
    Key? key,
    required this.inPins,
    required this.parentId,
    required this.parentPos,
  }) : super(key: key);

  final List<String> inPins;
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
          inPins.length,
          (index) {
            var pin = drawAreaData.objects[inPins[index]]! as DAOInputPin;
            Offset pos = const Offset(0.0, 24.0);
            if (index == 1) {
              pos = const Offset(0.0, 74.0);
            }

            return DrawPin(
              type: pin.type,
              parentId: parentId,
              id: inPins[index],
              parentPos: parentPos + pos,
            );
          },
        ),
      ),
    );
  }
}
