part of components;

class PseudoWire extends StatelessWidget {
  const PseudoWire({
    Key? key,
    required this.startPos,
    required this.endPos,
  }) : super(key: key);

  final Offset startPos;
  final Offset endPos;

  @override
  Widget build(BuildContext context) {
    return CustomPaint(
      painter: PseudoWirePainter(
        startPos,
        endPos,
      ),
    );
  }
}

class PseudoWirePainter extends CustomPainter {
  final Offset startPos;
  final Offset endPos;

  Path path = Path();

  PseudoWirePainter(
    this.startPos,
    this.endPos,
  );

  Paint get linePaint => Paint()
    ..color = MyTheme.wirePseudoColor
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
  bool shouldRepaint(PseudoWirePainter oldDelegate) {
    return oldDelegate.startPos != startPos || oldDelegate.endPos != endPos;
  }
}
