import 'package:bess/components/component_type.dart';
import 'package:bess/data/draw_area/draw_area_data.dart';
import 'package:bess/data/mouse_data.dart';
import 'package:bess/themes.dart';
import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

class DrawArea extends StatefulWidget {
  const DrawArea({Key? key}) : super(key: key);

  @override
  State<DrawArea> createState() => _DrawAreaState();
}

class _DrawAreaState extends State<DrawArea>
    with AutomaticKeepAliveClientMixin<DrawArea> {
  @override
  bool get wantKeepAlive => true;

  @override
  void initState() {
    super.initState();
  }

  void onDrawAreaClick(DrawAreaData data) {
    data.setSelectedItemId("");
  }

  @override
  Widget build(BuildContext context) {
    final drawAreaData = Provider.of<DrawAreaData>(context);
    final mouseData = Provider.of<MouseData>(context, listen: false);
    super.build(context);
    return GestureDetector(
      onTap: () {
        onDrawAreaClick(drawAreaData);
      },
      child: InteractiveViewer(
        maxScale: 4.0,
        child: MouseRegion(
          onHover: (e) {
            mouseData.setMousePos(e.localPosition);
          },
          child: Container(
            decoration: BoxDecoration(
              image: DecorationImage(
                colorFilter: ColorFilter.mode(
                  MyTheme.backgroundColor,
                  BlendMode.colorBurn,
                ),
                image: const AssetImage("assets/images/grid.png"),
                repeat: ImageRepeat.repeat,
              ),
            ),
            child: Stack(
              children: [
                ...List.generate(
                  drawAreaData.components.length,
                  (index) {
                    if (index >= drawAreaData.components.length) {
                      return const SizedBox(width: 0, height: 0);
                    }
                    var item = drawAreaData.components.values.elementAt(index);
                    if(item.properties.type != ComponentType.pin) {
                      return item.draw(context);
                    } else {
                      return const SizedBox();
                    }
                  },
                )
              ],
            ),
          ),
        ),
      ),
    );
  }
}
