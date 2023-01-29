
import 'package:bess/components/component_type.dart';
import 'package:bess/data/draw_area/draw_area_data.dart';
import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

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
    {
      "Nand Gate": (BuildContext context) {
        DrawAreaData data = Provider.of<DrawAreaData>(context, listen: false);
        data.setDrawingComponent(ComponentType.nandGate);
      }
    },
    {
      "Nor Gate": (BuildContext context) {
        DrawAreaData data = Provider.of<DrawAreaData>(context, listen: false);
        data.setDrawingComponent(ComponentType.norGate);
      }
    },
  ],
  ComponentType.io: [
    {
      "Input Button": (BuildContext context) {
        DrawAreaData data = Provider.of<DrawAreaData>(context, listen: false);
        data.setDrawingComponent(ComponentType.inputButton);
      }
    },
    {
      "Output State Probe": (BuildContext context) {
        DrawAreaData data = Provider.of<DrawAreaData>(context, listen: false);
        data.setDrawingComponent(ComponentType.outputStateProbe);
      }
    },
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
              const SizedBox(
                height: 5,
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
