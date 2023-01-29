import 'dart:ui';

import 'package:bess/components/component_type.dart';
import 'package:bess/components/components.dart';
import 'package:flutter/material.dart';

class TopBar extends StatelessWidget {
  const TopBar({super.key});

  @override
  Widget build(BuildContext context) {
    return Row(
      children: [
        Expanded(
          flex: 2,
          child: Row(
            children: [
              TextButton(
                child: const Text("File"),
                onPressed: () {},
              ),
              TextButton(
                child: const Text("Edit"),
                onPressed: () {},
              ),
              TextButton(
                child: const Text("Help"),
                onPressed: () {},
              ),
            ],
          ),
        ),
        const RightTopControls(),
      ],
    );
  }
}

Map<ComponentType, List<Map<String, void Function(BuildContext)>>> compFunc = {
  ComponentType.gate: [
    {"Nand Gate": NandGate.create},
    {"Nor Gate": NorGate.create},
  ],
  ComponentType.io: [
    {"Input Button": InputButton.create},
    {"Output State Probe": OutputStateProbe.create},
  ]
};

class RightTopControls extends StatefulWidget {
  const RightTopControls({
    super.key,
  });

  @override
  State<RightTopControls> createState() => _RightTopControlsState();
}

class _RightTopControlsState extends State<RightTopControls>
    with TickerProviderStateMixin {
  ComponentType _componentType = ComponentType.gate;
  late final TabController _tabController;
  @override
  void initState() {
    _tabController = TabController(length: compFunc.length, vsync: this);
    super.initState();
  }

  @override
  Widget build(BuildContext context) {
    return Expanded(
      child: Padding(
        padding: const EdgeInsets.symmetric(horizontal: 8.0),
        child: DefaultTabController(
          length: 2,
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.end,
            children: [
              TabBar(
                controller: _tabController,
                isScrollable: true,
                indicatorSize: TabBarIndicatorSize.tab,
                onTap: (ind) => setState(() {
                  _componentType = compFunc.keys.elementAt(ind);
                }),
                tabs: [
                  TextButton(
                    onPressed: () => setState(() {
                      _componentType = ComponentType.gate;
                      _tabController.animateTo(0);
                    }),
                    child: const Text("Gates"),
                  ),
                  TextButton(
                    onPressed: () => setState(() {
                      _componentType = ComponentType.io;
                      _tabController.animateTo(1);
                    }),
                    child: const Text("I/O"),
                  ),
                ],
              ),
              Row(
                mainAxisAlignment: MainAxisAlignment.end,
                children: compFunc[_componentType]!.map((comp) {
                  return TextButton(
                    onPressed: () {
                      comp.values.first(context);
                    },
                    child: Text(comp.keys.first),
                  );
                }).toList(),
              )
            ],
          ),
        ),
      ),
    );
  }
}
