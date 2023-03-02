part of components;

class WireWidget extends StatefulWidget {
  const WireWidget({
    Key? key,
    required this.id,
    required this.wireObj,
  }) : super(key: key);

  final String id;
  final Wire wireObj;

  @override
  State<WireWidget> createState() => _WireWidgetState();
}

class _WireWidgetState extends State<WireWidget> {
  @override
  Widget build(BuildContext context) {
    var drawAreaData = Provider.of<ProjectData>(context);

    var properties = widget.wireObj.properties as WireProperties;

    var controlPoint0 = properties.controlPointPositions[0];
    var controlPoint1 = properties.controlPointPositions[1];

    var startPos =
        drawAreaData.components[properties.startPinId]!.properties.pos;
    var endPos = drawAreaData.components[properties.endPinId]!.properties.pos;

    var state = (drawAreaData.components[properties.startPinId]!.properties
            as PinProperties)
        .state;

    return Stack(
      children: [
        ...(drawAreaData.selectedItemId == widget.id
            ? [
                ControlPointWidget(
                  index: 0,
                  parentId: widget.id,
                ),
                ControlPointWidget(
                  index: 1,
                  parentId: widget.id,
                ),
              ]
            : []),
        Material(
          animationDuration: Duration.zero,
          type: MaterialType.button,
          shape: CustomBorder(
            startPos,
            endPos,
            controlPoint0,
            controlPoint1,
          ),
          color: drawAreaData.selectedItemId == widget.id
              ? Colors.orange
              : state == DigitalState.high
                  ? Colors.red
                  : MyTheme.wireColor,
          child: InkWell(
            customBorder: CustomBorder(
              startPos,
              endPos,
              controlPoint0,
              controlPoint1,
            ),
            splashColor: Colors.orange,
            onTap: () => drawAreaData.setSelectedItemId(widget.id),
          ),
        ),
      ],
    );
  }
}

class CustomBorder extends OutlinedBorder {
  final Offset startPos;
  final Offset endPos;
  final Offset ctrlPoint1;
  final Offset ctrlPoint2;
  final double size = Defaults.wireSize;

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
    var ph = -10.0;
    if (high.dx < low.dx) {
      ph = 0;
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
