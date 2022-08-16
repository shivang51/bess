import 'package:bess/themes.dart';
import 'package:flutter/material.dart';

class SimulationArea extends StatelessWidget {
  const SimulationArea({Key? key}) : super(key: key);

  @override
  Widget build(BuildContext context) {
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
    );
  }
}
