import 'dart:math';

import 'package:bess/data/draw_area/draw_area_data.dart';
import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

double radians(double degrees) => degrees * (pi / 180);

class DrawWire extends StatefulWidget {
  const DrawWire({
    Key? key,
    required this.startPos,
    required this.endPos,
    required this.id,
  }) : super(key: key);

  final String id;
  final Offset startPos;
  final Offset endPos;

  @override
  State<DrawWire> createState() => _DrawWireState();
}

class _DrawWireState extends State<DrawWire> {
  late Offset ctrlPoint1;
  late Offset ctrlPoint2;

  @override
  void initState() {
    ctrlPoint1 = Offset(widget.endPos.dx, widget.startPos.dy - 20);
    ctrlPoint2 = Offset(widget.startPos.dx, widget.endPos.dy + 20);

    super.initState();
  }

  @override
  Widget build(BuildContext context) {
    DrawAreaData drawAreaData = Provider.of<DrawAreaData>(context);

    return Stack(
      children: [
        ...(drawAreaData.selectedItemId == widget.id
            ? [
                ControlPoint(
                  initPos: ctrlPoint1,
                  anchorPos: widget.startPos,
                  onUpdate: (pos) {
                    setState(() {
                      ctrlPoint1 = pos;
                    });
                  },
                ),
                ControlPoint(
                  initPos: ctrlPoint2,
                  anchorPos: widget.endPos,
                  onUpdate: (pos) {
                    setState(() {
                      ctrlPoint2 = pos;
                    });
                  },
                )
              ]
            : []),
        Material(
          animationDuration: Duration.zero,
          type: MaterialType.button,
          shape: CustomBorder(
            widget.startPos,
            widget.endPos,
            ctrlPoint1,
            ctrlPoint2,
          ),
          color: drawAreaData.selectedItemId == widget.id
              ? Colors.orange
              : Colors.green,
          child: InkWell(
            customBorder: CustomBorder(
              widget.startPos,
              widget.endPos,
              ctrlPoint1,
              ctrlPoint2,
            ),
            splashColor: Colors.orange,
            onTap: () => drawAreaData.setSelectedItemId(widget.id),
          ),
        ),
      ],
    );
  }
}

class ControlPoint extends StatefulWidget {
  const ControlPoint({
    Key? key,
    required this.onUpdate,
    required this.anchorPos,
    required this.initPos,
  }) : super(key: key);
  final Offset initPos;
  final Offset anchorPos;
  final void Function(Offset pos) onUpdate;

  @override
  State<ControlPoint> createState() => _ControlPointState();
}

class _ControlPointState extends State<ControlPoint> {
  late Offset pos;
  @override
  void initState() {
    pos = widget.initPos;
    // ============== Fix it, not working! ===========================
    // if (pos.dx < 0) {
    //   pos = Offset(0, pos.dy);
    // } else if (pos.dx > (0)) {
    //   pos = Offset(0, pos.dy);
    // }
    // if (pos.dy < 0) {
    //   pos = Offset(pos.dx, 0);
    // } else if (pos.dy > 720) {
    //   pos = Offset(pos.dx, 720);
    // }
    super.initState();
  }

  @override
  Widget build(BuildContext context) {
    return Positioned(
      left: pos.dx,
      top: pos.dy,
      child: GestureDetector(
        onPanUpdate: ((e) {
          setState(() {
            pos += e.delta;
            // if (pos.dx < 0) {
            //   pos = Offset(0, pos.dy);
            // } else if (pos.dx > MediaQuery.of(context).size.width) {
            //   pos = Offset(MediaQuery.of(context).size.width, pos.dy);
            // }
            // if (pos.dy < 0) {
            //   pos = Offset(pos.dx, 0);
            // } else if (pos.dy > MediaQuery.of(context).size.height) {
            //   pos = Offset(pos.dx, MediaQuery.of(context).size.height);
            // }
          });

          widget.onUpdate(pos);
        }),
        child: Container(
          width: 10,
          height: 10,
          color: Colors.teal,
          child: CustomPaint(
            painter: LinePainter(widget.anchorPos - pos),
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

class CustomBorder extends OutlinedBorder {
  final Offset startPos;
  final Offset endPos;
  final Offset ctrlPoint1;
  final Offset ctrlPoint2;
  final double size = 2;

  const CustomBorder(
      this.startPos, this.endPos, this.ctrlPoint1, this.ctrlPoint2,
      {BorderSide side = BorderSide.none})
      : super(side: side);

  Path customBorderPath(Rect rect) {
    Path path = Path();
    Offset high, low, ch, cl;
    if (startPos.dy < endPos.dy) {
      high = startPos;
      ch = ctrlPoint1;
      low = endPos;
      cl = ctrlPoint2;
    } else {
      low = startPos;
      cl = ctrlPoint1;
      high = endPos;
      ch = ctrlPoint2;
    }
    var ph = -10.0, pl = 10;
    if (high.dx < low.dx) {
      ph = 0;
      pl = -0;
    }

    if (low.dx < high.dx) {
      path.moveTo(high.dx, high.dy);
      path.cubicTo(
        ch.dx,
        ch.dy,
        cl.dx,
        cl.dy,
        low.dx,
        low.dy,
      );
      path.relativeLineTo(0, size);
      path.cubicTo(
        cl.dx + size,
        cl.dy + size,
        ch.dx + size,
        ch.dy + size,
        high.dx + size,
        high.dy + size,
      );
    } else if (ch.dx < high.dx) {
      path.moveTo(high.dx + ph, high.dy);
      path.cubicTo(
        ch.dx,
        ch.dy,
        cl.dx,
        cl.dy + size,
        low.dx,
        low.dy + size,
      );
      path.relativeLineTo(0, -size);
      path.cubicTo(
        cl.dx + size,
        cl.dy,
        ch.dx + size,
        ch.dy + size,
        high.dx,
        high.dy + size,
      );
    } else {
      path.moveTo(high.dx + ph, high.dy);
      path.cubicTo(ch.dx, ch.dy, cl.dx, cl.dy, low.dx, low.dy);
      path.relativeLineTo(0, size);
      path.relativeLineTo(-size, 0);
      ch = Offset(ch.dx - size, ch.dy + size);
      cl = Offset(cl.dx - size, cl.dy + size);
      path.cubicTo(cl.dx, cl.dy, ch.dx, ch.dy, high.dx - size, high.dy + size);
    }

    path.close();
    return path;
  }

  @override
  OutlinedBorder copyWith({BorderSide? side}) {
    return CustomBorder(
      side: side ?? this.side,
      startPos,
      endPos,
      ctrlPoint1,
      ctrlPoint2,
    );
  }

  @override
  EdgeInsetsGeometry get dimensions => EdgeInsets.all(side.width);

  @override
  Path getInnerPath(Rect rect, {TextDirection? textDirection}) =>
      customBorderPath(rect);

  @override
  Path getOuterPath(Rect rect, {TextDirection? textDirection}) =>
      customBorderPath(rect);

  @override
  void paint(Canvas canvas, Rect rect, {TextDirection? textDirection}) {
    switch (side.style) {
      case BorderStyle.none:
        break;
      case BorderStyle.solid:
        canvas.drawPath(customBorderPath(rect), Paint());
    }
  }

  @override
  ShapeBorder scale(double t) => CustomBorder(
        side: side.scale(t),
        startPos,
        endPos,
        ctrlPoint1,
        ctrlPoint2,
      );
}
