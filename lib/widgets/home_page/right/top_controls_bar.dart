import 'package:bess/procedures/simulation_procedures.dart';

import '../mwidgets/m_iconbutton.dart';
import 'package:flutter/material.dart';
import 'package:bess/data/app/app_data.dart';
import 'package:provider/provider.dart';
import 'package:bess/themes.dart';

class TopControlsBar extends StatelessWidget {
  const TopControlsBar({
    Key? key,
    required this.tabController,
  }) : super(key: key);

  final TabController tabController;

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.only(bottom: 1.0),
      child: Container(
        decoration: BoxDecoration(
          color: MyTheme.primaryBgColor,
        ),
        child: Row(
          mainAxisAlignment: MainAxisAlignment.spaceBetween,
          children: [
            TabBarControls(
              tabController: tabController,
            ),
            const RightControls(),
          ],
        ),
      ),
    );
  }
}

class TabBarControls extends StatefulWidget {
  const TabBarControls({
    Key? key,
    required this.tabController,
  }) : super(key: key);

  final TabController tabController;

  @override
  State<TabBarControls> createState() => _TabBarControlsState();
}

class _TabBarControlsState extends State<TabBarControls> {

  @override
  Widget build(BuildContext context) {
    var appData = Provider.of<AppData>(context);
    appData.setTabController(widget.tabController);
    return Row(
      children: [
        const SizedBox(
          width: 10.0,
        ),
        TabBarItem(
          icon: Icons.draw_rounded,
          onTap: () {
            setState(() {
              appData.tabController.index = 0;
            });
          },
          title: "Draw Area",
          selected: appData.tabController.index == 0,
        ),
        TabBarItem(
          icon: Icons.route_rounded,
          onTap: () {
            setState(() {
              appData.tabController.index = 1;
            });
          },
          title: "Simulation",
          selected: appData.tabController.index == 1,
        ),
      ],
    );
  }
}

class TabBarItem extends StatefulWidget {
  const TabBarItem({
    Key? key,
    required this.onTap,
    required this.title,
    this.selected = false,
    required this.icon,
  }) : super(key: key);

  final Function() onTap;
  final String title;
  final bool selected;
  final IconData icon;

  @override
  State<TabBarItem> createState() => _TabBarItemState();
}

class _TabBarItemState extends State<TabBarItem> {
  MouseCursor cursor = MouseCursor.defer;
  Color bgColor = Colors.transparent;
  @override
  Widget build(BuildContext context) {
    return MouseRegion(
      cursor: cursor,
      onEnter: (e) {
        setState(() {
          cursor = SystemMouseCursors.click;
          bgColor = MyTheme.secondaryBgColor;
        });
      },
      onExit: (e) {
        setState(() {
          cursor = SystemMouseCursors.basic;
          bgColor = Colors.transparent;
        });
      },
      child: GestureDetector(
        onTap: widget.onTap,
        child: Container(
          padding: const EdgeInsets.symmetric(vertical: 8.0, horizontal: 8.0),
          margin: const EdgeInsets.only(top: 4.0, left: 4.0, bottom: 4.0),
          decoration: BoxDecoration(
              color: widget.selected ? MyTheme.highlightColor : bgColor,
              borderRadius: BorderRadius.all(
                Radius.circular(MyTheme.primaryBorderRadius),
              )),
          child: Row(
            children: [
              Icon(
                widget.icon,
                size: 15.0,
                color: widget.selected ? Colors.white : Colors.white60,
              ),
              const SizedBox(
                width: 4.0,
              ),
              Text(
                widget.title,
                style: Theme.of(context).textTheme.titleSmall!.copyWith(
                      color: widget.selected ? Colors.white : Colors.white60,
                    ),
              ),
            ],
          ),
        ),
      ),
    );
  }
}

class RightControls extends StatelessWidget {
  const RightControls({
    Key? key,
  }) : super(key: key);

  @override
  Widget build(BuildContext context) {
    var appData = Provider.of<AppData>(context);
    return Row(
      mainAxisAlignment: MainAxisAlignment.end,
      children: [
        ...(appData.simulationState == SimulationState.stopped)
            ? [
                MIconButton(
                  onClicked: () {
                    SimProcedures.startSimulation(context);
                  },
                  icon: Icons.play_arrow_rounded,
                  color: Colors.green[400]!,
                  tooltip: "Start Simulation",
                )
              ]
            : [
                const SizedBox(
                  width: 4.0,
                ),
                MIconButton(
                  onClicked: () {},
                  color: Colors.teal[400]!,
                  icon: Icons.pause_rounded,
                  tooltip: "Pause Simulation",
                ),
                const SizedBox(
                  width: 4.0,
                ),
                MIconButton(
                  onClicked: () {
                    SimProcedures.stopSimulation(context);
                  },
                  color: Colors.red,
                  icon: Icons.stop_rounded,
                  tooltip: "Stop Simulation",
                ),
              ]
      ],
    );
  }
}
