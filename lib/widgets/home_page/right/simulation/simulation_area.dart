import 'package:bess/data/draw_area/draw_area_data.dart';
import 'package:bess/data/draw_area/objects/io/obj_input_button.dart';
import 'package:bess/data/draw_area/objects/io/obj_output_button.dart';
import 'package:bess/data/draw_area/objects/pins/obj_pin.dart';
import 'package:bess/data/draw_area/objects/types.dart';
import 'package:bess/data/draw_area/objects/wires/obj_wire.dart';
import 'package:bess/themes.dart';
import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../draw_area/drawer/draw_input_button.dart';
import '../draw_area/drawer/draw_nand_gate.dart';
import '../draw_area/drawer/draw_nor_gate.dart';
import '../draw_area/drawer/draw_output_probe.dart';
import '../draw_area/drawer/draw_wire.dart';

class SimulationArea extends StatelessWidget {
  const SimulationArea({Key? key}) : super(key: key);

  @override
  Widget build(BuildContext context) {
    DrawAreaData drawAreaData = Provider.of<DrawAreaData>(context);

    return Container(
      decoration: BoxDecoration(
        image: DecorationImage(
          colorFilter: ColorFilter.mode(
            MyTheme.backgroundColor,
            BlendMode.colorBurn,
          ),
          image: const AssetImage("assets/images/grid.png"),
          repeat: ImageRepeat.repeat,
        ),
      ),
      child: Stack(
        children: [
          ...List.generate(
            drawAreaData.objects.length,
            (index) {
              if (index >= drawAreaData.objects.length) {
                return const SizedBox(width: 0, height: 0);
              }

              var item = drawAreaData.objects.values.elementAt(index);
              if (item.type == DrawObjectType.nandGate) {
                return DrawNandGate(
                  id: item.id,
                  initialPos: const Offset(100.0, 100.0),
                  simulation: true,
                );
              } else if (item.type == DrawObjectType.norGate) {
                return DrawNorGate(
                  id: item.id,
                  initialPos: const Offset(100.0, 100.0),
                  simulation: true,
                );
              } else if (item.type == DrawObjectType.wire) {
                var item_ = item as DAOWire;

                if (drawAreaData.objects[item_.startPinId] == null ||
                    drawAreaData.objects[item_.endPinId] == null) {
                  drawAreaData.objects.remove(item.id);
                  return const SizedBox();
                }

                var sPin =
                    drawAreaData.objects[item_.startPinId] as DrawAreaPin;
                var ePin = drawAreaData.objects[item_.endPinId] as DrawAreaPin;

                return DrawWire(
                  id: item_.id,
                  endPos: ePin.pos!,
                  startPos: sPin.pos!,
                  simulation: true,
                );
              } else if (item.type == DrawObjectType.inputButton) {
                var item_ = item as DAOInputButton;
                return DrawInputButton(
                  id: item_.id,
                  pinId: item_.pinId,
                  simulation: true,
                );
              } else if (item.type == DrawObjectType.outputProbe) {
                var item_ = item as DAOOutputProbe;
                return DrawOutputProbe(
                  id: item_.id,
                  pinId: item_.pinId,
                  simulation: true,
                );
              } else {
                return const SizedBox(width: 0, height: 0);
              }
            },
          )
        ],
      ),
    );
  }
}
