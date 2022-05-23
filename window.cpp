#include "window.hpp"
#include "qcustomplot.h"
#include <QDateTime>
#include <QString>
#include <QCloseEvent>
#include <cstdlib>
#include <cstdio>


RangeBox::RangeBox()
  : layout(new QGridLayout())
{
  labels = {"10mV", "20mV", "50mV", "0.1V", "0.2V", "0.5V", "1V", "2V", "5V", "10V", "20V", "50V"};
  for(auto p: labels)
    addItem(tr(p.c_str()));

  this->setLayout(layout);
}


TypeBox::TypeBox()
  : layout(new QGridLayout())
{
  labels = {"Off", "X", "Y", "Z0", "Z1", "Z2", "Z3", "Z4", "Z5", "Z6", "Z7"};
  for(auto p: labels)
    addItem(tr(p.c_str()));

  this->setLayout(layout);
}


ChannelWindow::ChannelWindow(UNIT * u, Worker * w, QCustomPlot * p, QCPColorMap * c)
  : layout(new QGridLayout)
  , unit(u)
  , worker(w)
  , loop(new QEventLoop)
  , customPlot(p)
  , colorMap(c)
{
  for(int i = 0; i<PS4000A_MAX_CHANNELS; i++)
    {
      layout->addWidget(create_group_box(i), 0, i);
    }

  Update_Button = new QPushButton(tr("&Update"));
  layout->addWidget(Update_Button, 1, PS4000A_MAX_CHANNELS-1);

  setLayout(layout);

  connect(Update_Button, SIGNAL(clicked()), this, SLOT(set_channels_of_pico()));
  connect(worker, SIGNAL(unit_stopped_signal()), loop, SLOT(quit()));

  voltages = {10.0, 20.0, 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0, 20000.0, 50000.0};
}


QGroupBox * ChannelWindow::create_group_box(int ch)
{
  QString str('A' + ch);
  channelBox[ch] = new QGroupBox("     " + str);
  channelBox[ch]->setCheckable(true);
  channelBox[ch]->setChecked(false);
  connect(channelBox[ch], SIGNAL(clicked(bool)), this, SLOT(set_channels()));

  QVBoxLayout * channelLayout = new QVBoxLayout();

  RangeBox_Obj[ch] = new RangeBox();
  channelLayout->addWidget(RangeBox_Obj[ch]);
  connect(RangeBox_Obj[ch], SIGNAL(currentIndexChanged(int)), this, SLOT(set_channels()));

  Offset_SpinBox_Obj[ch] = new QDoubleSpinBox();
  channelLayout->addWidget(Offset_SpinBox_Obj[ch]);
  connect(Offset_SpinBox_Obj[ch], SIGNAL(valueChanged(double)), this, SLOT(set_channels()));

  TypeBox_Obj[ch] = new TypeBox();
  channelLayout->addWidget(TypeBox_Obj[ch]);
  connect(TypeBox_Obj[ch], SIGNAL(currentIndexChanged(int)), this, SLOT(set_channels()));

  channelBox[ch]->setLayout(channelLayout);

  return channelBox[ch];
}


void ChannelWindow::update_axis()
{
  double xRange, yRange;

  for(int ch = 0; ch < unit->channelCount; ch++)
    {
      int range = unit->channelSettings[ch].range;

      if(unit->channelSettings[ch].enabled &&
         unit->channelSettings[ch].mode == X)
        xRange = voltages[range];
      if(unit->channelSettings[ch].enabled &&
              unit->channelSettings[ch].mode == Y)
        yRange = voltages[range];
    }

  colorMap->data()->setRange(QCPRange(-xRange,xRange), QCPRange(-yRange, yRange));
  customPlot->rescaleAxes();
  customPlot->replot();
}


void ChannelWindow::set_channels()
{
  for(int ch = 0; ch<PS4000A_MAX_CHANNELS; ch++)
    {
      unit->channelSettings[ch].enabled   = channelBox[ch]->isChecked();
      unit->channelSettings[ch].range     = (PICO_CONNECT_PROBE_RANGE)RangeBox_Obj[ch]->currentIndex();
      get_offset_bounds(ch);
      unit->channelSettings[ch].offset    = (float)Offset_SpinBox_Obj[ch]->value();
      unit->channelSettings[ch].mode      = (MODE)TypeBox_Obj[ch]->currentIndex();
    }
  update_axis();
}


void ChannelWindow::set_channels_of_pico()
{
  bool stream_was_running_flag = g_streamIsRunning ? true : false;

  g_streamIsRunning = false;
  if(stream_was_running_flag)
    loop->exec();

  for (int ch = 0; ch < unit->channelCount; ch++)
    {
      PICO_STATUS status = ps4000aSetChannel(unit->handle,
                                             (PS4000A_CHANNEL)(PS4000A_CHANNEL_A + ch),
                                             unit->channelSettings[ch].enabled,
                                             unit->channelSettings[ch].coupling,
                                             unit->channelSettings[ch].range,
                                             unit->channelSettings[ch].offset);

      printf(status?"SetDefaults:ps4000aSetChannel------ 0x%08lx \n":"", (long unsigned int)status);
    }

  if(stream_was_running_flag)
    emit(do_work(*unit));

  this->close();
}


void ChannelWindow::get_offset_bounds(int ch)
{
  float min, max;
  ps4000aGetAnalogueOffset(unit->handle,
                           unit->channelSettings[ch].range,
                           unit->channelSettings[ch].coupling,
                           &max,
                           &min);
  unit->channelSettings[ch].maxOffset = max;
  unit->channelSettings[ch].minOffset = min;

  Offset_SpinBox_Obj[ch]->setMaximum((double)max);
  Offset_SpinBox_Obj[ch]->setMinimum((double)min);
}


Window::Window()
  : customPlot(new QCustomPlot())
  , colorMap(new QCPColorMap(customPlot->xAxis, customPlot->yAxis))
  , toolBar(new QToolBar())
  , unit(new UNIT)
  , Worker_Obj(new Worker)
  , ChannelWindow_Obj(new ChannelWindow(unit, Worker_Obj, customPlot, colorMap))
  , loop(new QEventLoop)
{
  Worker_Obj->moveToThread(&Thread_Obj);
  Thread_Obj.start();
  qRegisterMetaType<UNIT>();

  g_streamIsRunning = false;
  counter           = 0;
}


void Window::open_unit()
{
  PICO_STATUS status = ps4000aOpenUnit(&(unit->handle), NULL);

  if(status != PICO_OK)
    printf("Unable to open device.\n");
}


void Window::get_unit_info()
{
  unit->minRange        = PS4000A_10MV;
  unit->maxRange        = PS4000A_50V;
  unit->channelCount    = PS4000A_MAX_CHANNELS;
  ps4000aMaximumValue(unit->handle, &unit->maxSampleValue);
  ps4000aMinimumValue(unit->handle, &unit->minSampleValue);


  char description [11][25]= {
    "Driver Version",
    "USB Version",
    "Hardware Version",
    "Variant Info",
    "Serial",
    "Cal Date",
    "Kernel",
    "Digital H/W",
    "Analogue H/W",
    "Firmware 1",
    "Firmware 2"};

  int8_t line[80];
  int16_t r;
  PICO_STATUS status;

  if (unit->handle)
    {

      for (int i = 0; i < 11; i++)
        {
          status = ps4000aGetUnitInfo(unit->handle,
                                      line,
                                      sizeof(line),
                                      &r,
                                      i);
          printf("%s: %s\n", description[i], line);

          if(status != PICO_OK)
            {
              printf("\n\nError in get_info()!\n");
              break;
            }
        }
    }
}


void Window::start()
{
  open_unit();
  get_unit_info();
  set_channels();
  set_channels_of_pico();
  set_main_window();
  set_actions();
  set_connections();
  this->show();
  ChannelWindow_Obj->show();
}


void Window::set_channels()
{

  for (int ch = 0; ch < unit->channelCount; ch++)
    {
      unit->channelSettings[ch].range         = (PICO_CONNECT_PROBE_RANGE)PS4000A_5V;
      unit->channelSettings[ch].enabled       = true;
      unit->channelSettings[ch].bufferEnabled = false;
      unit->channelSettings[ch].mode          = OFF;
      unit->channelSettings[ch].offset        = 0.0;
      unit->channelSettings[ch].maxOffset     = 0.0;
      unit->channelSettings[ch].minOffset     = 0.0;
      unit->channelSettings[ch].coupling      = (PS4000A_COUPLING)false;
    }
}


void Window::set_channels_of_pico()
{
  for(int ch = 0; ch < unit->channelCount; ch++)
    {
      PICO_STATUS status = ps4000aSetChannel(unit->handle,
                                 (PS4000A_CHANNEL)(PS4000A_CHANNEL_A + ch),
                                 unit->channelSettings[ch].enabled,
                                 unit->channelSettings[ch].coupling,
                                 unit->channelSettings[ch].range,
                                 unit->channelSettings[ch].offset);

      printf(status?"SetDefaults:ps4000aSetChannel------ 0x%08lx \n":"", (long unsigned int)status);
    }
}


void Window::get_allowed_offset()
{
  for(int ch = 0; ch < unit->channelCount; ch++)
    {
      ps4000aGetAnalogueOffset(unit->handle,
                               unit->channelSettings[ch].range,
                               unit->channelSettings[ch].coupling,
                               &unit->channelSettings[ch].maxOffset,
                               &unit->channelSettings[ch].minOffset);
    }
}


void Window::set_main_window()
{
  setCentralWidget(customPlot);
  resize(800, 800);
  addToolBar(toolBar);

  customPlot->axisRect()->setupFullAxesBox(true);
  customPlot->setInteraction(QCP::iRangeDrag, true);
  customPlot->setInteraction(QCP::iRangeZoom, true);

  colorMap->setGradient(QCPColorGradient::gpGrayscale);
  colorMap->setDataRange(QCPRange(-1.0, 1.0));
  colorMap->data()->setSize(200, 200);
  colorMap->data()->fill(0.0);

  colorMap->data()->setRange(QCPRange(-1000.0,1000.0), QCPRange(-1000.0,1000.0));



  customPlot->replot();
  //this->show();
}


void Window::set_actions()
{
  streamButton = new QPushButton();
  streamButton->setText(g_streamIsRunning ? "&Stop" : "&Start");
  toolBar->addWidget(streamButton);

  saveButton = new QPushButton(tr("&Save"));
  toolBar->addWidget(saveButton);

  videoButton = new QPushButton(tr("&Video"));
  toolBar->addWidget(videoButton);

  scaleOffsetBox = new QDoubleSpinBox();
  scaleOffsetBox->setMaximum(20);
  scaleOffsetBox->setMinimum(-20);
  scaleOffsetBox->setSingleStep(0.1);
  scaleOffsetBox->setValue(0);
  scaleOffsetBoxAction = toolBar->addWidget(scaleOffsetBox);

  scaleAmplitudeBox = new QDoubleSpinBox();
  scaleAmplitudeBox->setMaximum(20);
  scaleAmplitudeBox->setMinimum(0);
  scaleAmplitudeBox->setSingleStep(0.1);
  scaleAmplitudeBox->setValue(1);
  scaleAmplitudeBoxAction = toolBar->addWidget(scaleAmplitudeBox);

  sizeBox = new QSpinBox();
  sizeBox->setMaximum(350);
  sizeBox->setMinimum(50);
  sizeBox->setSingleStep(10);
  sizeBox->setValue(200);
  sizeBoxAction = toolBar->addWidget(sizeBox);

  show_ChannelMenu = new QAction(tr("&Channel"));
  menuBar()->addAction(show_ChannelMenu);
}


void Window::set_connections()
{
  connect(show_ChannelMenu, SIGNAL(triggered()), this, SLOT(show_channel_menu_slot()));

  connect(ChannelWindow_Obj, SIGNAL(do_work(UNIT)), this, SLOT(stream_button_slot()));

  connect(streamButton, SIGNAL(clicked()), this, SLOT(stream_button_slot()));
  connect(this, SIGNAL(do_work(UNIT)), Worker_Obj, SLOT(stream_data(UNIT)));
  connect(Worker_Obj, SIGNAL(data(double,double,double,double,double,double,double,double)), this, SLOT(data(double,double,double,double,double,double,double,double)));
  connect(sizeBox, SIGNAL(valueChanged(int)), this, SLOT(set_size_slot(int)));
  connect(Worker_Obj, SIGNAL(unit_stopped_signal()), loop, SLOT(quit()));
}


void Window::show_channel_menu_slot()
{
  ChannelWindow_Obj->show();
}


void Window::set_size_slot(int size)
{
  colorMap->data()->setSize(size, size);
  customPlot->replot();
}


void Window::stream_button_slot()
{
  if(!g_streamIsRunning)
    {
      emit(do_work(*unit));
      g_streamIsRunning = true;
    }
  else if(g_streamIsRunning)
    g_streamIsRunning = false;

  streamButton->setText(g_streamIsRunning ? "&Stop" : "&Start");

}


void Window::data(double x, double y,
                  double z0, double z1,
                  double z2, double z3,
                  double z4, double z5)
{
  int xInd, yInd;
  colorMap->data()->coordToCell(x,y,&xInd,&yInd);
  colorMap->data()->setCell(xInd, yInd, 1.0);
  counter++;
  if(counter%1000 == 0)
    customPlot->replot();
}


void Window::contextMenuEvent(QContextMenuEvent *event)
{
  QMenu menu(this);
  menu.exec(event->globalPos());
}


void Window::closeEvent(QCloseEvent *event)
{
  if(g_streamIsRunning == true)
    {
      g_streamIsRunning = false;
      loop->exec();
    }
  ps4000aCloseUnit(unit->handle);
  Thread_Obj.quit();
  Thread_Obj.wait();
  QCoreApplication::quit();
  event->accept();
}
