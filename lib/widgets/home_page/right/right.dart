import 'package:bess/data/app/app_data.dart';
import 'package:bess/themes.dart';
import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import './draw_area/draw_area.dart';
import './simulation/simulation_area.dart';
import './top_controls_bar.dart';

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
    var appData = Provider.of<AppData>(context, listen: false);
    appData.setTabController(tabController);
    return DefaultTabController(
      length: 2,
      child: Column(
        children: [
          const TopControlsBar(),
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
