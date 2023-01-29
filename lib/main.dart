import 'package:flutter/material.dart';

import 'pages/home_page.dart';
import 'pages/splash_page.dart';

void main() {
  runApp(const MyApp());
}

class MyApp extends StatelessWidget {
  const MyApp({Key? key}) : super(key: key);

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'Basic Electrical Simulation Software',
      debugShowCheckedModeBanner: false,
      theme: ThemeData(
        useMaterial3: true,
        brightness: Brightness.dark,
      ),
      routes: {
        HomePage.id: ((context) => const HomePage()),
        SplashPage.id: ((context) => const SplashPage()),
      },
      initialRoute: SplashPage.id,

      // home: const Home(),
    );
  }
}
