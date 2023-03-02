import 'package:bess/data/app/app_data.dart';
import 'package:bess/data/project_data/project_data.dart';
import 'package:bess/data/mouse_data.dart';
import 'package:bess/widgets/home_page/bottom/bottom_bar.dart';
import 'package:bess/widgets/home_page/left/left.dart';
import 'package:bess/widgets/home_page/right/right.dart';
import 'package:bess/widgets/home_page/top/top_bar.dart';
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
        ChangeNotifierProvider<AppData>(create: (_) => AppData()),
        ChangeNotifierProvider<ProjectData>(create: (_) => ProjectData()),
        ChangeNotifierProvider<MouseData>(create: (_) => MouseData()),
      ],
      child: Scaffold(
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
            ),
            const BottomBar(),
          ],
        ),
      ),
    );
  }
}
