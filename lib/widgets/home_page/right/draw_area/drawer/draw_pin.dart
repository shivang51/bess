import 'package:bess/data/draw_area/objects/pins/obj_pin.dart';
import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import 'package:uuid/uuid.dart';

import 'package:bess/data/draw_area/draw_area_data.dart';
import 'package:bess/data/draw_area/objects/types.dart';
import 'package:bess/data/draw_area/objects/wires/obj_wire.dart';
import 'package:bess/data/mouse_data.dart';

import './painters/line_painter.dart';

class DrawPin extends StatefulWidget {
  const DrawPin({
    Key? key,
    required this.parentId,
    required this.pinPos,
    required this.id,
    required this.type,
    this.width = 20.0,
    this.simulation = false,
  }) : super(key: key);

  final String id;
  final DrawObjectType type;
  final String parentId;
  final Offset pinPos;
  final double width;
  final bool simulation;

  @override
  State<DrawPin> createState() => _DrawPinState();
}

class _DrawPinState extends State<DrawPin> {
  bool hovered = false;
  Offset pos = Offset.zero;

  void postFrameCallback(DrawAreaData drawAreaData) {
    WidgetsBinding.instance.addPostFrameCallback((_) {
      drawAreaData.setProperty(
        widget.id,
        widget.type,
        DrawElementProperty.pos,
        pos,
      );
    });
  }

  @override
  void initState() {
    super.initState();
  }

  @override
  Widget build(BuildContext context) {
    DrawAreaData drawAreaData = Provider.of<DrawAreaData>(context);
    var mouseData = Provider.of<MouseData>(context);
    var widget_ = drawAreaData.objects[widget.id]! as DrawAreaPin;
    pos = widget.pinPos;
    Offset offset = const Offset(0.0, 1.0);

    if (widget.type == DrawObjectType.pinOut) {
      // pos += const Offset(40.0, 0.0);
      offset = const Offset(20.0, 1.0);
    }

    postFrameCallback(drawAreaData);

    return GestureDetector(
      onTap: () {
        if (drawAreaData.drawingElement != DrawElement.connection) {
          drawAreaData.setConnStartData(widget.parentId, widget.id);
          drawAreaData.setDrawingElement(DrawElement.connection);
        } else {
          drawAreaData.setDrawingElement(DrawElement.none);
          final id = const Uuid().v4();
          drawAreaData.setProperty(drawAreaData.connStartData.startPinId, DrawObjectType.pin, DrawElementProperty.addConnection, widget.id,);
          drawAreaData.addObject(
            id,
            DAOWire(
              id: id,
              name: "Wire 1",
              startGateId: drawAreaData.connStartData.startGateId,
              startPinId: drawAreaData.connStartData.startPinId,
              endGateId: widget.parentId,
              endPinId: widget.id,
            ),
          );
        }
      },
      child: MouseRegion(
        onEnter: (e) {
          setState(() {
            hovered = true;
          });
        },
        onExit: (e) => {
          setState(() {
            hovered = false;
          })
        },
        cursor: hovered ? SystemMouseCursors.click : MouseCursor.defer,
        child: Stack(
          children: [
            ...((widget.simulation)
                ? [
                    SizedBox(
                      height: 28,
                      width: widget.width,
                    ),
                    Positioned(
                      top: -5,
                      left: widget.width / 2 - 5,
                      child: Text(
                        widget_.state == DigitalState.high ? "1" : "0",
                      ),
                    ),
                  ]
                : [const SizedBox()]),
            Positioned(
              top: widget.simulation ? 13 : null,
              child: Container(
                decoration: BoxDecoration(
                  color: hovered ? Colors.red : Colors.red[300]!,
                ),
                width: widget.width,
                height: 2.0,
                child: drawAreaData.drawingElement == DrawElement.connection &&
                        drawAreaData.connStartData.startPinId == widget.id
                    ? Wire(
                        startPos: offset,
                        endPos: mouseData.mousePos - pos + offset,
                      )
                    : null,
              ),
            ),
          ],
        ),
      ),
    );
  }
}

class Wire extends StatelessWidget {
  const Wire({
    Key? key,
    required this.startPos,
    required this.endPos,
  }) : super(key: key);

  final Offset startPos;
  final Offset endPos;

  @override
  Widget build(BuildContext context) {
    return CustomPaint(
      painter: LinePainter(
        startPos,
        endPos,
      ),
    );
  }
}
