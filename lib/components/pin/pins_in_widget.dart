part of components;

class PinsInWidget extends StatelessWidget {
  const PinsInWidget({Key? key, required this.ids}) : super(key: key);

  final List<String> ids;

  @override
  Widget build(BuildContext context) {
    ProjectData drawAreaData = Provider.of<ProjectData>(context, listen: false);
    var properties =
        drawAreaData.components[ids[0]]!.properties as PinProperties;
    double height = Defaults.defaultGateSize.dy;
    if (drawAreaData.components[properties.parentId]!.properties.type ==
        ComponentType.notGate) {
      height = Defaults.notGateSize.dy;
    }
    return SizedBox(
      height: height,
      width: properties.width,
      child: Column(
        mainAxisAlignment: MainAxisAlignment.spaceAround,
        children: List.generate(
          ids.length,
          (index) {
            var pin = drawAreaData.components[ids[index]]! as Pin;
            return pin.draw(context);
          },
        ),
      ),
    );
  }
}
