import 'package:bess/themes.dart';
import 'package:flutter/material.dart';

class NandPainter extends ShapeBorder {
  final Offset startPos = const Offset(0.0, 0.0);

  final Paint nandPaint = Paint()
    ..color = MyTheme.gateBorderColor
    ..style = PaintingStyle.stroke
    ..strokeWidth = 2;

  Path nandGatePath(Rect rect) {
    Path path = Path();
    double x = 0;
    double y = 0;
    x += 50;
    path.lineTo(x, y);
    y += 100;
    path.arcToPoint(
      Offset(x, y),
      radius: const Radius.circular(1),
      largeArc: true,
    );
    x -= 50;
    path.lineTo(x, y);
    path.lineTo(0, 0);
    path.addOval(Rect.fromCircle(center: const Offset(105, 50), radius: 5.0));
    path.close();
    return path;
  }

  @override
  EdgeInsetsGeometry get dimensions => const EdgeInsets.all(1.0);

  @override
  Path getInnerPath(Rect rect, {TextDirection? textDirection}) {
    return Path()
      ..fillType = PathFillType.evenOdd
      ..addPath(getOuterPath(rect), Offset.zero);
  }

  @override
  Path getOuterPath(Rect rect, {TextDirection? textDirection}) {
    return nandGatePath(rect);
  }

  @override
  void paint(Canvas canvas, Rect rect, {TextDirection? textDirection}) {
    canvas.drawPath(nandGatePath(rect), nandPaint);
  }

  @override
  ShapeBorder scale(double t) {
    return NandPainter().scale(t);
  }
}