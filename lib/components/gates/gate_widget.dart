part of components;

class GateWidget extends StatefulWidget {
  const GateWidget({
    Key? key,
    required this.id,
    required this.gateObj,
    required this.shapeBorder,
  }) : super(key: key);

  final String id;
  final Gate gateObj;
  final ShapeBorder shapeBorder;

  @override
  State<GateWidget> createState() => _GateWidgetState();
}

class _GateWidgetState extends State<GateWidget> {
  var pos = Offset.zero;
  void updatePosition(DrawAreaData data, Offset delta) {
    var newPos = pos + delta;
    if (newPos.dx < 0 || newPos.dy < 0) return;
    setState(() {
      pos = newPos;
      widget.gateObj.properties.pos = pos;
      data.updateComponentProperty(widget.id, ComponentPropertyType.pos, pos);
    });
  }

  @override
  Widget build(BuildContext context) {
    var drawAreaData = Provider.of<DrawAreaData>(context);
    // var appData = Provider.of<AppData>(context);

    pos = widget.gateObj.properties.pos;
    var selected = widget.id == drawAreaData.selectedItemId;
    return Positioned(
      left: pos.dx,
      top: pos.dy,
      child: Stack(
        children: [
          selected
              ? Container(
                  width: 150,
                  height: 100,
                  padding: const EdgeInsets.all(8.0),
                  decoration: BoxDecoration(
                    color: Theme.of(context).highlightColor,
                  ),
                )
              : const SizedBox(),
          GestureDetector(
            onPanUpdate: (e) => updatePosition(drawAreaData, e.delta),
            child: Stack(
              children: [
                Pin.drawInput(
                    (widget.gateObj.properties as GateProperties).inputPins),
                Row(
                  children: [
                    const SizedBox(
                      width: 20.0,
                    ),
                    SizedBox(
                      width: 110.0,
                      height: 100.0,
                      child: Material(
                        type: MaterialType.button,
                        animationDuration: Duration.zero,
                        shape: widget.shapeBorder,
                        color: MyTheme.componentBgColor,
                        child: InkWell(
                          customBorder: widget.shapeBorder,
                          splashColor: Colors.white.withAlpha(10),
                          hoverColor: Colors.transparent,
                          onTap: () =>
                              drawAreaData.setSelectedItemId(widget.id),
                          child: Column(
                            crossAxisAlignment: CrossAxisAlignment.end,
                            mainAxisAlignment: MainAxisAlignment.center,
                            children: [
                              Padding(
                                padding: const EdgeInsets.all(18.0),
                                child: Text(
                                  widget.gateObj.properties.name,
                                  style: Theme.of(context).textTheme.titleSmall,
                                  overflow: TextOverflow.ellipsis,
                                ),
                              ),
                            ],
                          ),
                        ),
                      ),
                    ),
                    Pin.drawOutput((widget.gateObj.properties as GateProperties)
                        .outputPins),
                  ],
                ),
              ],
            ),
          ),
        ],
      ),
    );
  }
}
