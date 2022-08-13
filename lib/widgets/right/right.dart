import 'package:bess/themes.dart';
import 'package:bess/widgets/right/draw_area/draw_area.dart';
import 'package:bess/widgets/right/simulation/simulation_area.dart';
import 'package:bess/widgets/right/top_controls_bar.dart';
import 'package:flutter/material.dart';

class Right extends StatefulWidget {
  const Right({
    Key? key,
  }) : super(key: key);

  @override
  State<Right> createState() => _RightState();
}

class _RightState extends State<Right> with SingleTickerProviderStateMixin {
  late TabController tabController;

  @override
  void initState() {
    super.initState();
    tabController = TabController(vsync: this, length: 2);
  }

  @override
  void dispose() {
    tabController.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return DefaultTabController(
      length: 2,
      child: Column(
        children: [
          TopControlsBar(
            tabController: tabController,
          ),
          Expanded(
            child: Container(
              height: double.infinity,
              decoration: BoxDecoration(
                color: MyTheme.primaryBgColor,
                borderRadius: BorderRadius.only(
                  bottomRight: Radius.circular(MyTheme.primaryBorderRadius),
                ),
              ),
              child: TabBarView(
                controller: tabController,
                children: const [
                  DrawArea(),
                  SimulationArea(),
                ],
              ),
            ),
          ),
        ],
      ),
    );
  }
}
