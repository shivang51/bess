part of components;

class ComponentGenerator {
  static void generate(ComponentType componentType, BuildContext context,
      {Offset pos = Offset.zero}) {
    switch (componentType) {
      case ComponentType.nandGate:
        NandGate.create(context, pos);
        break;
      case ComponentType.norGate:
        NorGate.create(context, pos);
        break;
      case ComponentType.notGate:
        NotGate.create(context, pos);
        break;
      case ComponentType.inputButton:
        InputButton.create(context, pos);
        break;
      case ComponentType.outputStateProbe:
        OutputStateProbe.create(context, pos);
        break;
      default:
    }
  }
}
