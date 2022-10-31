import 'package:bess/components/components.dart';
import 'package:bess/data/draw_area/draw_area_data.dart';
import 'package:bess/themes.dart';
import 'package:flutter/material.dart';
import 'package:font_awesome_flutter/font_awesome_flutter.dart';
import 'package:provider/provider.dart';
import 'package:uuid/uuid.dart';

class ComponentExplorer extends StatelessWidget {
  const ComponentExplorer({Key? key}) : super(key: key);

  final Uuid uuid = const Uuid();

  @override
  Widget build(BuildContext context) {
    DrawAreaData drawAreaData = Provider.of<DrawAreaData>(
      context,
      listen: false,
    );
    return Container(
      padding: const EdgeInsets.all(8.0),
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
                  FontAwesomeIcons.bezierCurve,
                  color: MyTheme.primaryTextColor.withAlpha(220),
                  size: 13.0,
                ),
                const SizedBox(
                  width: 8.0,
                ),
                Text(
                  "Component Explorer",
                  style: Theme.of(context).textTheme.titleSmall!.copyWith(
                        color: MyTheme.primaryTextColor.withAlpha(220),
                      ),
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
            child: GridView.count(
              primary: false,
              crossAxisSpacing: 10,
              crossAxisCount: 2,
              mainAxisSpacing: 10,
              childAspectRatio: 4.0,
              children: [
                //Nand Gate Component
                TextButton(
                  onPressed: () {
                    NandGate.create(context);
                  },
                  child: const Padding(
                    padding: EdgeInsets.symmetric(vertical: 4.0),
                    child: Text("NAND"),
                  ),
                ),
                TextButton(
                  onPressed: () {
                    NorGate.create(context);
                  },
                  child: const Padding(
                    padding: EdgeInsets.symmetric(vertical: 4.0),
                    child: Text("NOR"),
                  ),
                ),
                TextButton(
                  onPressed: () {
                    InputButton.create(context);
                  },
                  child: const Padding(
                    padding: EdgeInsets.symmetric(vertical: 4.0),
                    child: Text("Input"),
                  ),
                ),
                TextButton(
                  onPressed: () {
                    OutputStateProbe.create(context);
                  },
                  child: const Padding(
                    padding: EdgeInsets.symmetric(vertical: 4.0),
                    child: Text("Output"),
                  ),
                ),
              ],
            ),
          )
        ],
      ),
    );
  }
}
