import 'package:bess/data/draw_area/objects/gates/obj_nor_gate.dart';
import 'package:bess/data/draw_area/objects/types.dart';
import 'package:bess/themes.dart';

import './input_pins.dart';
import './output_pins.dart';
import './painters/nor_painter.dart';
import 'package:bess/data/draw_area/draw_area_data.dart';

import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

class DrawNorGate extends StatefulWidget {
  const DrawNorGate({
    Key? key,
    required this.id,
    required this.initialPos,
    this.simulation = false,
  }) : super(key: key);

  final String id;
  final Offset initialPos;
  final bool simulation;

  @override
  State<DrawNorGate> createState() => _DrawNorGateState();
}

class _DrawNorGateState extends State<DrawNorGate> {
  late Offset pos;

  late DrawAreaData drawAreaData;
  late DAONorGate gate;

  @override
  void initState() {
    pos = widget.initialPos;

    super.initState();
  }

  @override
  Widget build(BuildContext context) {
    drawAreaData = Provider.of<DrawAreaData>(context);
    gate = drawAreaData.objects[widget.id]! as DAONorGate;

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
        child: Stack(
          children: [
            InputPins(
              inPins: gate.inputPins,
              parentId: widget.id,
              parentPos: pos,
              width: 40.0,
              simulation: widget.simulation,
            ),
            Row(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                const SizedBox(
                  width: 20.0,
                ),
                SizedBox(
                  width: 110.0,
                  height: 100.0,
                  child: Material(
                    type: MaterialType.button,
                    animationDuration: Duration.zero,
                    shape: NorPainter(),
                    color: selected
                        ? MyTheme.highlightColor.withAlpha(200)
                        : Colors.transparent,
                    child: InkWell(
                      customBorder: NorPainter(),
                      splashColor: Colors.orange,
                      hoverColor: Colors.transparent,
                      onTap: () => drawAreaData.setSelectedItemId(widget.id),
                      child: Column(
                        children: [
                          Text(
                            "NOR Gate",
                            style: Theme.of(context).textTheme.titleSmall,
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
          ],
        ),
      ),
    );
  }
}
