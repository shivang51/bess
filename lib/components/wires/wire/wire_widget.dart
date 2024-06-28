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

    var startPos =
        drawAreaData.components[properties.startPinId]!.properties.pos;
    var endPos = drawAreaData.components[properties.endPinId]!.properties.pos;

    var state = (drawAreaData.components[properties.startPinId]!.properties
            as PinProperties)
        .state;

    return BezierCurveBernstein(
      start: startPos,
      end: endPos,
      weight: Defaults.wireSize,
      color: MyTheme.wireColor,
    );
  }
}

class BezierCurveBernstein extends StatelessWidget {
  final Offset start;
  final Offset end;
  final double weight;
  final void Function()? onClick;
  final void Function()? onRightClick;
  final void Function()? onMouseHover;
  final void Function()? onMouseEnter;
  final void Function()? onMouseExit;
  final Color? color;

  const BezierCurveBernstein({
    super.key,
    required this.start,
    required this.end,
    required this.weight,
    this.onClick,
    this.onRightClick,
    this.onMouseHover,
    this.onMouseEnter,
    this.onMouseExit,
    this.color,
  });

  (Offset, double) getSizeRot(Offset start, Offset end) {
    double angle = atan2(end.dy - start.dy, end.dx - start.dx);
    angle = double.parse(angle.toStringAsFixed(6));
    double distance =
        sqrt(pow(end.dy - start.dy, 2) + pow(end.dx - start.dx, 2));
    double sX = max(distance, weight);
    double sY = weight;

    return (Offset(sX, sY), angle);
  }

  int calculateSegements(Offset start, Offset end, Size viewPortSize) {
    var dis = (end - start)
        .scale(1 / viewPortSize.width, 1 / viewPortSize.height)
        .distance;

    // make it dependent on scale (zoom)
    return (dis / 0.005).ceil();
  }

  List<(Offset, Offset, double)> generateBezierPoints(BuildContext context) {
    final Offset p0 = start;
    final Offset p3 = end;
    var dx = (p3.dx - p0.dx) * 0.5;
    final Offset p1 = Offset(p0.dx + dx, p0.dy);
    final Offset p2 = Offset(p3.dx - dx, p3.dy);
    List<(Offset, Offset, double)> points = [];
    int numPoints = calculateSegements(p0, p3, MediaQuery.of(context).size);
    var prev = p0;
    for (int i = 1; i <= numPoints; i++) {
      double t = i / numPoints;
      var t_ = 1 - t;

      var b0 = p0 * pow(t_, 3).toDouble();
      var b1 = p1 * pow(t_, 2).toDouble() * pow(t, 1).toDouble() * 3;
      var b2 = p2 * pow(t_, 1).toDouble() * pow(t, 2).toDouble() * 3;
      var b3 = p3 * pow(t, 3).toDouble();

      var bP = b0 + b1 + b2 + b3;

      var (size, angle) = getSizeRot(prev, bP);
      prev = bP;
      points.add((bP, size, angle));
    }

    return points;
  }

  @override
  Widget build(BuildContext context) {
    var bezierPoints = generateBezierPoints(context);
    return Stack(
      children: bezierPoints.map((point_) {
        var (point, size_, angle) = point_;
        return Positioned(
          left: point.dx - size_.dx / 2,
          top: point.dy - size_.dy / 2,
          width: size_.dx,
          height: size_.dy,
          child: MouseRegion(
            onHover: (_) => onMouseHover?.call(),
            onEnter: (_) => onMouseEnter?.call(),
            onExit: (_) => onMouseExit?.call(),
            child: GestureDetector(
              onTap: () => onClick?.call(),
              onSecondaryTap: () => onRightClick?.call(),
              child: Transform.rotate(
                angle: angle,
                child: Container(
                  decoration: BoxDecoration(
                    color: color ?? Colors.black,
                    shape: BoxShape.rectangle,
                  ),
                ),
              ),
            ),
          ),
        );
      }).toList(),
    );
  }
}
