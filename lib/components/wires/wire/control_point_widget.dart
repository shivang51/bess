import 'package:bess/components/component_properties.dart';
import 'package:bess/components/wires/wire/wire_properties.dart';
import 'package:bess/data/draw_area/draw_area_data.dart';
import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

class ControlPointWidget extends StatefulWidget {
  const ControlPointWidget(
      {Key? key, required this.parentId, required this.index})
      : super(key: key);

  final String parentId;
  final int index;

  @override
  State<ControlPointWidget> createState() => _ControlPointWidgetState();
}

class _ControlPointWidgetState extends State<ControlPointWidget> {
  @override
  Widget build(BuildContext context) {
    DrawAreaData drawAreaData = Provider.of<DrawAreaData>(context);

    var parentProperties =
        drawAreaData.components[widget.parentId]!.properties as WireProperties;
    var pinId = (widget.index == 0)
        ? parentProperties.startPinId
        : parentProperties.endPinId;

    var anchorPoint = drawAreaData.components[pinId]!.properties.pos;
    var pos = parentProperties.controlPointPositions[widget.index];

    return Positioned(
      left: pos.dx,
      top: pos.dy,
      child: GestureDetector(
        onPanUpdate: ((e) {
          setState(() {
            pos += e.delta;
          });
          parentProperties.controlPointPositions[widget.index] = pos;
          drawAreaData.updateComponentProperty(
              widget.parentId,
              ComponentPropertyType.controlPointPosition,
              {"index": widget.index, "position": pos});
        }),
        child: Container(
          decoration: BoxDecoration(
            color: Colors.teal,
            shape: BoxShape.circle,
            border: Border.all(color: Theme.of(context).indicatorColor),
          ),
          width: 12,
          height: 12,
          child: CustomPaint(
            painter: LinePainter(anchorPoint - pos),
          ),
        ),
      ),
    );
  }
}

class LinePainter extends CustomPainter {
  final Offset endPos;

  Path path = Path();

  LinePainter(
    this.endPos,
  );

  Paint get linePaint => Paint()
    ..color = Colors.teal
    ..strokeWidth = 2
    ..style = PaintingStyle.stroke
    ..strokeJoin = StrokeJoin.round
    ..strokeCap = StrokeCap.round;

  @override
  void paint(Canvas canvas, Size size) {
    path = Path();
    path.moveTo(5, 5);
    path.lineTo(endPos.dx, endPos.dy + 1);
    canvas.drawPath(path, linePaint);
  }

  @override
  bool shouldRepaint(LinePainter oldDelegate) {
    return oldDelegate.endPos != endPos;
  }
}
