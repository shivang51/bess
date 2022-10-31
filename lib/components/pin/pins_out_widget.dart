part of components;

class PinsOutWidget extends StatelessWidget {
  const PinsOutWidget({
    Key? key,
    required this.ids,
  }) : super(key: key);

  final List<String> ids;

  @override
  Widget build(BuildContext context) {
    DrawAreaData drawAreaData =
    Provider.of<DrawAreaData>(context, listen: false);
    var properties = drawAreaData.components[ids[0]]!.properties as PinProperties;
    return SizedBox(
      height: 100.0,
      width: properties.width,
      child: Column(
        mainAxisAlignment: MainAxisAlignment.spaceAround,
        children: List.generate(
          ids.length,
              (index) {
            var pin = drawAreaData.components[ids[index]] as Pin;
            return pin.draw(context);
          },
        ),
      ),
    );
  }
}