import 'package:bess/data/project_data/project_data.dart';
import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

class ProjectProcedures {
  static void saveProject(BuildContext context) {
    var projectData = Provider.of<ProjectData>(context, listen: false);
  }

  static void loadProject(BuildContext context) {
    // load the binary file into a project structure
  }
}
