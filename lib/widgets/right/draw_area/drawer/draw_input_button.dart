import 'package:flutter/material.dart';

import 'package:bess/data/draw_area/objects/draw_objects.dart';
import 'package:bess/themes.dart';
import 'package:bess/widgets/right/draw_area/drawer/draw_pin.dart';

class DrawInputButton extends StatefulWidget {
  const DrawInputButton({
    Key? key,
    required this.id,
    required this.pinId,
  }) : super(key: key);

  final String id;
  final String pinId;

  @override
  State<DrawInputButton> createState() => _DrawInputButtonState();
}

class _DrawInputButtonState extends State<DrawInputButton> {
  late Offset pos;
  late bool high;
  bool hovered = false;

  @override
  void initState() {
    pos = Offset.zero;
    high = false;
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
        onTap: () {
          setState(() {
            high = !high;
          });
        },
        child: Row(
          children: [
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
                    shape: BoxShape.circle,
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
            DrawPin(
              parentId: widget.id,
              pinPos: pos + const Offset(45.0, 11.5),
              id: widget.pinId,
              type: DrawObject.pinOut,
            ),
          ],
        ),
      ),
    );
  }
}
