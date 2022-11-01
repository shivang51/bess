import 'package:bess/components/component.dart';
import 'package:bess/components/component_type.dart';
import 'package:flutter/material.dart';
import 'package:font_awesome_flutter/font_awesome_flutter.dart';
import 'package:provider/provider.dart';

import 'package:bess/data/draw_area/draw_area_data.dart';
import 'package:bess/themes.dart';

import 'project_explorer_item.dart';

class ProjectExplorer extends StatelessWidget {
  const ProjectExplorer({
    Key? key,
  }) : super(key: key);

  @override
  Widget build(BuildContext context) {
    return Container(
      padding: const EdgeInsets.all(4.0),
      decoration: BoxDecoration(
        color: MyTheme.primaryBgColor,
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Padding(
            padding: const EdgeInsets.only(left: 8.0),
            child: Row(
              children: [
                Icon(
                  FontAwesomeIcons.listCheck,
                  color: MyTheme.primaryTextColor.withAlpha(220),
                  size: 13.0,
                ),
                const SizedBox(
                  width: 8.0,
                ),
                Text(
                  "Project Explorer",
                  style: Theme.of(context)
                      .textTheme
                      .titleSmall!
                      .copyWith(color: MyTheme.primaryTextColor.withAlpha(220)),
                ),
              ],
            ),
          ),
          Container(
            width: double.infinity,
            height: 1.0,
            color: Colors.grey,
            margin: const EdgeInsets.only(
              top: 10.0,
              bottom: 10.0,
              right: 26.0,
              left: 10.0,
            ),
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
    );
  }
}
