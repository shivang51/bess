import 'package:bess/data/draw_area/draw_area_data.dart';
import 'package:bess/data/mouse_data.dart';
import 'package:bess/themes.dart';
import 'package:bess/widgets/left/left.dart';
import 'package:bess/widgets/right/right.dart';
import 'package:bess/widgets/top/topbar.dart';
import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

class HomePage extends StatelessWidget {
  const HomePage({
    Key? key,
  }) : super(key: key);

  static const String id = "home_page_id";

  @override
  Widget build(BuildContext context) {
    return MultiProvider(
      providers: [
        ChangeNotifierProvider<DrawAreaData>(create: (_) => DrawAreaData()),
        ChangeNotifierProvider<MouseData>(create: (_) => MouseData()),
      ],
      child: Scaffold(
        backgroundColor: MyTheme.backgroundColor,
        body: Column(
          children: [
            const TopBar(),
            Expanded(
              child: Row(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: const [
                  Expanded(
                    flex: 2,
                    child: Left(),
                  ),
                  Expanded(
                    flex: 8,
                    child: Right(),
                  ),
                ],
              ),
            )
          ],
        ),
      ),
    );
  }
}
