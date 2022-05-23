// #include <stdio.h>
// #include <sys/types.h>
// #include <string.h>
// #include <termios.h>
// #include <sys/ioctl.h>
// #include <sys/types.h>
// #include <unistd.h>
// #include <stdlib.h>
// #include <ctype.h>
//#include <curses.h>


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
