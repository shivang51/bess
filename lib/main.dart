import 'package:bess/pages/home_page.dart';
import 'package:bess/pages/splash_page.dart';
import 'package:bess/themes.dart';
import 'package:flutter/material.dart';
import 'package:google_fonts/google_fonts.dart';

void main() {
  runApp(const MyApp());
}

class MyApp extends StatelessWidget {
  const MyApp({Key? key}) : super(key: key);

  // This widget is the root of your application.
  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'Best Electrical Simulation Software',
      theme: ThemeData(
        useMaterial3: true,
        cardTheme: mCardTheme,
        textButtonTheme: mTextButtonThemeData,
        listTileTheme: mListTileThemeData,
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
    );
  }
}
