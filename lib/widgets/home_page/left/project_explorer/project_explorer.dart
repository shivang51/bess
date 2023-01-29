import 'package:bess/components/component.dart';
import 'package:bess/components/component_type.dart';
import 'package:flutter/material.dart';
import 'package:font_awesome_flutter/font_awesome_flutter.dart';
import 'package:provider/provider.dart';

import 'package:bess/data/draw_area/draw_area_data.dart';

import 'project_explorer_item.dart';

class ProjectExplorer extends StatelessWidget {
  const ProjectExplorer({
    Key? key,
  }) : super(key: key);

  @override
  Widget build(BuildContext context) {
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(8.0),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Row(
              mainAxisAlignment: MainAxisAlignment.center,
              children: [
                const Icon(
                  FontAwesomeIcons.microscope,
                  size: 13.0,
                ),
                const SizedBox(
                  width: 8.0,
                ),
                Text(
                  "Project Explorer",
                  style: Theme.of(context).textTheme.titleMedium,
                ),
              ],
            ),
            const SizedBox(
              height: 10.0,
            ),
            Expanded(
              child: Consumer<DrawAreaData>(
                builder: (context, data, child) {
                  List<Component> components = data.components.values
                      .where((element) =>
                          element.properties.type != ComponentType.pin &&
                          element.properties.type != ComponentType.wire)
                      .toList();
                  return ListView.builder(
                    padding: const EdgeInsets.only(right: 10.0),
                    itemBuilder: (context, index) {
                      var item = components.elementAt(index);
                      return Padding(
                        padding: const EdgeInsets.all(2.0),
                        child: ProjectExplorerItem(
                          id: item.properties.id,
                          obj: item,
                        ),
                      );
                    },
                    itemCount: components.length,
                  );
                },
              ),
            ),
          ],
        ),
      ),
    );
  }
}
