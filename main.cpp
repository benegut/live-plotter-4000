#include <QProcess>
#include <QApplication>
#include <QStringList>
#include <QThread>
#include "window.hpp"

#include <libps4000a-1.0/ps4000aApi.h>
#ifndef PICO_STATUS
#include <libps4000a-1.0/PicoStatus.h>
#endif



int main(int argc, char **argv) {

  QApplication app(argc, argv);
  Window window;
  window.start();
  return app.exec();
}
