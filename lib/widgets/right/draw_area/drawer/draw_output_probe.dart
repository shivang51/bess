import 'package:bess/data/draw_area/objects/draw_objects.dart';
import 'package:bess/themes.dart';
import 'package:flutter/material.dart';

import 'draw_pin.dart';

class DrawOutputProbe extends StatefulWidget {
  const DrawOutputProbe({
    Key? key,
    required this.id,
    required this.pinId,
    this.high = false,
  }) : super(key: key);

  final String id;
  final String pinId;
  final bool high;

  @override
  State<DrawOutputProbe> createState() => _DrawOutputProbeState();
}

class _DrawOutputProbeState extends State<DrawOutputProbe> {
  late Offset pos;
  bool hovered = false;
  @override
  void initState() {
    pos = Offset.zero;
    super.initState();
  }

  @override
  Widget build(BuildContext context) {
    return Positioned(
      left: pos.dx,
      top: pos.dy,
      child: GestureDetector(
        onPanUpdate: (e) {
          setState(() {
            pos += e.delta;
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
                message: widget.high ? "High" : "Low",
                child: Container(
                  width: 25.0,
                  height: 25.0,
                  decoration: BoxDecoration(
                    border: Border.all(color: MyTheme.borderColor),
                    color: widget.high ? Colors.red : Colors.transparent,
                  ),
                  child: Center(
                    child: Text(
                      widget.high ? "H" : "L",
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
