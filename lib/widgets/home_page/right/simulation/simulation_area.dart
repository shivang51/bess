import 'package:bess/components/component_type.dart';
import 'package:bess/data/draw_area/draw_area_data.dart';
import 'package:bess/themes.dart';
import 'package:flutter/material.dart';
import 'package:provider/provider.dart';


class SimulationArea extends StatelessWidget {
  const SimulationArea({Key? key}) : super(key: key);

  @override
  Widget build(BuildContext context) {
    DrawAreaData drawAreaData = Provider.of<DrawAreaData>(context);

    return Container(
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
    );
  }
}
