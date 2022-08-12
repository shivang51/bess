import 'package:flutter/material.dart';

class LinePainter extends CustomPainter {
  final Offset startPos;
  final Offset endPos;

  Path path = Path();

  LinePainter(
    this.startPos,
    this.endPos,
  );

  Paint get linePaint => Paint()
    ..color = Colors.amber[200]!
    ..strokeWidth = 3
    ..style = PaintingStyle.stroke
    ..strokeJoin = StrokeJoin.round
    ..strokeCap = StrokeCap.round;

  @override
  void paint(Canvas canvas, Size size) {
    path = Path();
    path.moveTo(startPos.dx, startPos.dy);
    double cX1 = startPos.dx + (endPos.dx - startPos.dx) / 2;
    double cX2 = (endPos.dx + startPos.dx) / 2;
    path.cubicTo(cX1, startPos.dy, cX2, endPos.dy, endPos.dx, endPos.dy);
    canvas.drawPath(path, linePaint);
  }

  @override
  bool shouldRepaint(LinePainter oldDelegate) {
    return oldDelegate.startPos != startPos || oldDelegate.endPos != endPos;
  }
}
