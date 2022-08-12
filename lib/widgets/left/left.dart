import 'package:bess/widgets/left/component_explorer/component_explorer.dart';
import 'package:bess/widgets/left/project_explorer/project_explorer.dart';
import 'package:flutter/material.dart';

class Left extends StatelessWidget {
  const Left({
    Key? key,
  }) : super(key: key);

  @override
  Widget build(BuildContext context) {
    return Padding(
        padding: const EdgeInsets.only(
          top: 1.0,
          left: 1.0,
          right: 1.0,
        ),
        child: Column(
          children: const [
            Expanded(flex: 7, child: ProjectExplorer()),
            SizedBox(
              height: 1.0,
            ),
            Expanded(flex: 3, child: ComponentExplorer()),
          ],
        ));
  }
}
