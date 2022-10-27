import 'package:bess/data/draw_area/objects/gates/obj_nand_gate.dart';
import 'package:bess/data/draw_area/objects/types.dart';

import './input_pins.dart';
import './output_pins.dart';

import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import 'package:bess/data/draw_area/draw_area_data.dart';
import 'package:bess/themes.dart';

import 'painters/nand_painter.dart';

class DrawNandGate extends StatefulWidget {
  const DrawNandGate({
    Key? key,
    required this.id,
    required this.initialPos,
    this.simulation = false,
  }) : super(key: key);

  final String id;
  final Offset initialPos;
  final bool simulation;

  @override
  State<DrawNandGate> createState() => _DrawNandGateState();
}

class _DrawNandGateState extends State<DrawNandGate> {
  late Offset pos;
  late DrawAreaData drawAreaData;
  late DAONandGate gate;

  @override
  Widget build(BuildContext context) {
    drawAreaData = Provider.of<DrawAreaData>(context);
    gate = drawAreaData.objects[widget.id]! as DAONandGate;
    bool selected = widget.id == drawAreaData.selectedItemId;
    pos = gate.pos ?? widget.initialPos;
    return Positioned(
      left: pos.dx,
      top: pos.dy,
      child: GestureDetector(
        onPanUpdate: widget.simulation
            ? null
            : (e) {
                if (pos.dx + e.delta.dx < 0 || pos.dy + e.delta.dy < 0) {
                  return;
                }
                setState(() {
                  pos += e.delta;
                  drawAreaData.setProperty(widget.id, DrawObjectType.nandGate,
                      DrawElementProperty.pos, pos);
                });
              },
        child: Row(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            InputPins(
              inPins: gate.inputPins,
              parentId: widget.id,
              parentPos: pos,
              simulation: widget.simulation,
            ),
            SizedBox(
              width: 110.0,
              height: 100.0,
              child: Material(
                type: MaterialType.button,
                animationDuration: Duration.zero,
                shape: NandPainter(),
                color: selected
                    ? MyTheme.highlightColor.withAlpha(200)
                    : Colors.transparent,
                child: InkWell(
                  customBorder: NandPainter(),
                  splashColor: Colors.orange,
                  hoverColor: Colors.transparent,
                  onTap: () => drawAreaData.setSelectedItemId(widget.id),
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Text(
                        drawAreaData.objects[widget.id]!.name,
                        style: Theme.of(context).textTheme.titleSmall,
                        overflow: TextOverflow.ellipsis,
                      ),
                    ],
                  ),
                ),
              ),
            ),
            OutputPins(
              outPins: gate.outputPins,
              parentId: widget.id,
              parentPos: pos,
              simulation: widget.simulation,
            ),
          ],
        ),
      ),
    );
  }
}
