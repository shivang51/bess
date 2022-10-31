import 'package:bess/themes.dart';
import 'package:flutter/material.dart';
import 'package:google_fonts/google_fonts.dart';

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
      theme: ThemeData(
        useMaterial3: true,
        cardTheme: mCardTheme,
        textButtonTheme: mTextButtonThemeData,
        listTileTheme: mListTileThemeData,
        colorScheme: ColorScheme.fromSeed(
          seedColor: const Color(0xff6750a4),
        ).copyWith(
          brightness: Brightness.dark,
        ),
        textTheme: TextTheme(
          titleLarge: GoogleFonts.roboto(
            textStyle: TextStyle(
              color: MyTheme.primaryTextColor,
              fontSize: 18.0,
            ),
          ),
          titleMedium: GoogleFonts.roboto(
            textStyle: TextStyle(
              color: MyTheme.primaryTextColor,
              fontSize: 15.0,
            ),
          ),
          titleSmall: GoogleFonts.roboto(
            textStyle: TextStyle(
              color: MyTheme.primaryTextColor,
              fontSize: 13.0,
            ),
          ),
        ),
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
