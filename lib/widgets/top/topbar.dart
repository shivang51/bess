import 'package:bess/themes.dart';
import 'package:flutter/material.dart';

class TopBar extends StatelessWidget {
  const TopBar({
    Key? key,
  }) : super(key: key);

  @override
  Widget build(BuildContext context) {
    return Container(
      decoration: BoxDecoration(
        color: MyTheme.primaryBgColor,
      ),
      padding: const EdgeInsets.all(3.0),
      child: Row(
        children: const [
          SizedBox(
            height: 20.0,
          )
        ],
      ),
    );
  }
}
