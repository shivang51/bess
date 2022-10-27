import 'package:bess/data/draw_area/draw_area_data.dart';
import 'package:bess/data/draw_area/objects/pins/obj_pin.dart';
import 'package:bess/data/draw_area/objects/types.dart';
import 'package:bess/themes.dart';
import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import 'draw_pin.dart';

class DrawOutputProbe extends StatefulWidget {
  const DrawOutputProbe({
    Key? key,
    required this.id,
    required this.pinId,
    this.simulation = false,
  }) : super(key: key);

  final String id;
  final String pinId;
  final bool simulation;

  @override
  State<DrawOutputProbe> createState() => _DrawOutputProbeState();
}

class _DrawOutputProbeState extends State<DrawOutputProbe> {
  late Offset pos;
  bool hovered = false;
  bool high = false;
  @override
  void initState() {
    pos = Offset.zero;
    super.initState();
  }

  @override
  Widget build(BuildContext context) {
    DrawAreaData drawAreaData = Provider.of<DrawAreaData>(context);
    var obj = drawAreaData.objects[widget.id]!;
    pos = obj.pos ?? Offset.zero;

    high = (drawAreaData.objects[widget.pinId] as DrawAreaPin).state == DigitalState.high;

    return Positioned(
      left: pos.dx,
      top: pos.dy,
      child: GestureDetector(
        onPanUpdate: widget.simulation
            ? null
            : (e) {
                setState(() {
                  pos += e.delta;
                  drawAreaData.setProperty(
                    widget.id,
                    DrawObjectType.inputButton,
                    DrawElementProperty.pos,
                    pos,
                  );
                });
              },
        child: Row(
          children: [
            DrawPin(
              parentId: widget.id,
              pinPos: pos + const Offset(0.0, 11.5),
              id: widget.pinId,
              type: DrawObjectType.pinIn,
            ),
            MouseRegion(
              onEnter: (e) {
                setState(() {
                  hovered = true;
                });
              },
              onExit: (e) {
                setState(() {
                  hovered = false;
                });
              },
              cursor: hovered ? SystemMouseCursors.click : MouseCursor.defer,
              child: Tooltip(
                message: high ? "High" : "Low",
                child: Container(
                  width: 25.0,
                  height: 25.0,
                  decoration: BoxDecoration(
                    border: Border.all(color: MyTheme.borderColor),
                    color: high ? Colors.red : Colors.transparent,
                  ),
                  child: Center(
                    child: Text(
                      high ? "H" : "L",
                      style: Theme.of(context).textTheme.titleSmall,
                      overflow: TextOverflow.ellipsis,
                    ),
                  ),
                ),
              ),
            ),
          ],
        ),
      ),
    );
  }
}
