#include "application.h"

int main() {
  Bess::Application *app = new Bess::Application();
  app->run();
  delete app;
  return 0;
}
