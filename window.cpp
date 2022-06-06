#include "window.hpp"
#include "qcustomplot.h"
#include <QDateTime>
#include <QString>
#include <QCloseEvent>
#include <cstdio>
#include <iostream>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>


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
  labels = {"Off", "X", "Y", "Z0", "Z1", "Z2", "Z3", "Z4", "Z5", "Z6", "Z7", "Z8", "Z9"};
  for(auto p: labels)
    addItem(tr(p.c_str()));

  this->setLayout(layout);
}


ChannelWindow::ChannelWindow(UNIT * u, Worker * w, QCustomPlot * t, QCustomPlot * xy, QCPColorMap * c)
  : unit(u)
  , worker(w)
  , loop(new QEventLoop)
  , timePlot(t)
  , xyPlot(xy)
  , colorMap(c)
  , layout(new QGridLayout)
  , unit_Box(new QGroupBox[_UNITCOUNT_])
  , unit_Layout(new QGridLayout[_UNITCOUNT_])
  , channelBox(new QGroupBox*[_UNITCOUNT_])
  , RangeBox_Obj(new RangeBox*[_UNITCOUNT_])
  , Offset_SpinBox_Obj(new QDoubleSpinBox*[_UNITCOUNT_])
  , TypeBox_Obj(new TypeBox*[_UNITCOUNT_])
{
  for(int16_t i = 0; i < _UNITCOUNT_; i++)
    {
      channelBox[i]         = new QGroupBox[unit[i].channelCount];
      RangeBox_Obj[i]       = new RangeBox[unit[i].channelCount];
      Offset_SpinBox_Obj[i] = new QDoubleSpinBox[unit[i].channelCount];
      TypeBox_Obj[i]        = new TypeBox[unit[i].channelCount];
    }

  for(int16_t u = 0; u < _UNITCOUNT_; u++)
    {
      for(int ch = 0; ch < unit[u].channelCount; ch++)
        {
          unit_Layout[u].addWidget(create_group_box(u,ch), 0, ch);
        }
      unit_Box[u].setLayout(unit_Layout+u);
      std::stringstream ss;
      ss << "HANDLE: " << unit[u].handle << "    SERIAL: " << unit[u].serial;
      unit_Box[u].setTitle(ss.str().c_str());
      layout->addWidget(unit_Box+u, u, 0);
    }

  Update_Button = new QPushButton(tr("&Update"));
  layout->addWidget(Update_Button, _UNITCOUNT_, 0);

  setLayout(layout);

  connect(Update_Button, SIGNAL(clicked()), this, SLOT(set_channels_of_pico()));
  connect(worker, SIGNAL(unit_stopped_signal()), loop, SLOT(quit()));

  voltages = {10.0, 20.0, 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0, 20000.0, 50000.0};
}


QGroupBox * ChannelWindow::create_group_box(int u, int ch)
{
  QString str('A' + ch);
  channelBox[u][ch].setTitle("    " + str);
  channelBox[u][ch].setCheckable(true);
  channelBox[u][ch].setChecked(false);
  connect(channelBox[u]+ch, SIGNAL(clicked(bool)), this, SLOT(set_channels()));

  QVBoxLayout * channelLayout = new QVBoxLayout();

  channelLayout->addWidget(RangeBox_Obj[u]+ch);
  connect(RangeBox_Obj[u]+ch, SIGNAL(currentIndexChanged(int)), this, SLOT(set_channels()));

  channelLayout->addWidget(Offset_SpinBox_Obj[u]+ch);
  connect(Offset_SpinBox_Obj[u]+ch, SIGNAL(valueChanged(double)), this, SLOT(set_channels()));

  channelLayout->addWidget(TypeBox_Obj[u]+ch);
  connect(TypeBox_Obj[u]+ch, SIGNAL(currentIndexChanged(int)), this, SLOT(set_channels()));

  channelBox[u][ch].setLayout(channelLayout);

  return channelBox[u]+ch;
}


void ChannelWindow::update_axis()
{
  double xRange, yRange, fRange = 0.0;

  for(int u = 0; u < _UNITCOUNT_; u++)
    {
      for(int ch = 0; ch < unit[u].channelCount; ch++)
        {
          int range = unit[u].channelSettings[ch].range;

          if(unit[u].channelSettings[ch].enabled &&
             unit[u].channelSettings[ch].mode == X)
            xRange = voltages[range];
          if(unit[u].channelSettings[ch].enabled &&
             unit[u].channelSettings[ch].mode == Y)
            yRange = voltages[range];
          if(unit[u].channelSettings[ch].enabled &&
             unit[u].channelSettings[ch].mode >= Z0)
             fRange = voltages[range]>fRange?voltages[range]:fRange;
        }
    }

  colorMap->data()->setRange(QCPRange(-xRange,xRange), QCPRange(-yRange, yRange));
  xyPlot->rescaleAxes();
  xyPlot->replot();
  timePlot->yAxis->setRange(-fRange, fRange);
  timePlot->replot();
}


void ChannelWindow::set_channels()
{
  for(int u = 0; u < _UNITCOUNT_; u++)
    {
      for(int ch = 0; ch < unit[u].channelCount; ch++)
        {
          unit[u].channelSettings[ch].enabled   = channelBox[u][ch].isChecked();
          unit[u].channelSettings[ch].range     = (PICO_CONNECT_PROBE_RANGE)RangeBox_Obj[u][ch].currentIndex();
          get_offset_bounds(u, ch);
          unit[u].channelSettings[ch].offset    = (float)Offset_SpinBox_Obj[u][ch].value();
          unit[u].channelSettings[ch].mode      = (MODE)TypeBox_Obj[u][ch].currentIndex();
        }
    }
  update_axis();
}


void ChannelWindow::set_channels_of_pico()
{
  bool stream_was_running_flag = g_streamIsRunning ? true : false;

  g_streamIsRunning = false;
  if(stream_was_running_flag)
    loop->exec();

  for(int u = 0; u < _UNITCOUNT_; u++)
    {
      for (int ch = 0; ch < unit[u].channelCount; ch++)
        {
          PICO_STATUS status = ps4000aSetChannel(unit[u].handle,
                                                 (PS4000A_CHANNEL)(PS4000A_CHANNEL_A + ch),
                                                 unit[u].channelSettings[ch].enabled,
                                                 unit[u].channelSettings[ch].coupling,
                                                 unit[u].channelSettings[ch].range,
                                                 unit[u].channelSettings[ch].offset);

          printf(status?"SetDefaults:ps4000aSetChannel------ 0x%08lx \n":"", (long unsigned int)status);
        }
    }

  if(stream_was_running_flag)
    emit(do_work(unit));

  this->close();
}


void ChannelWindow::get_offset_bounds(int u, int ch)
{
  float min, max;
  ps4000aGetAnalogueOffset(unit[u].handle,
                           unit[u].channelSettings[ch].range,
                           unit[u].channelSettings[ch].coupling,
                           &max,
                           &min);
  unit[u].channelSettings[ch].maxOffset = max;
  unit[u].channelSettings[ch].minOffset = min;

  Offset_SpinBox_Obj[u][ch].setMaximum((double)max);
  Offset_SpinBox_Obj[u][ch].setMinimum((double)min);
}







Window::Window()
  : toolBar(new QToolBar())
  , Worker_Obj(new Worker)
  , loop(new QEventLoop)
  , splitter(new QSplitter(this))
  , timePlot(new QCustomPlot)
  , xyPlot(new QCustomPlot)
  , colorMap(new QCPColorMap(xyPlot->xAxis, xyPlot->yAxis))
  , mathChannel_vec(QVector<double>(30))
{
  Worker_Obj->moveToThread(&Thread_Obj);
  Thread_Obj.start();
  qRegisterMetaType<UNIT>();
  qRegisterMetaType<std::vector<double>>();

  g_streamIsRunning = false;
  counter           = 0;

  for(int mode = X; mode != Z9 + 1; mode++)
    data_vec.push_back(0.0);

  colorMapData_ptr = &data_vec[Z0-1];
}


void Window::start()
{
  open_unit();
  get_unit_info();
  set_channels();
  set_channels_of_pico();
  ChannelWindow_Obj = new ChannelWindow(unit, Worker_Obj, timePlot, xyPlot, colorMap);
  set_main_window();
  set_actions();
  set_connections();
  this->show();
  ChannelWindow_Obj->show();
  GraphWindow_Obj = new GraphWindow(this);
  MathWindow_Obj = new MathWindow(this);
  ColorMapDataChooser_Obj = new ColorMapDataChooser(this);

}


void Window::open_unit()
{
  PICO_STATUS status;

  int16_t serialLth = 100;
  int8_t * serials = new int8_t[serialLth];
  ps4000aEnumerateUnits(&_UNITCOUNT_, serials, &serialLth);
  std::cout << "\nNumber of Pico's found: " << _UNITCOUNT_ << std::endl;

  unit = new UNIT[_UNITCOUNT_];

  for(int16_t i = 0; i < _UNITCOUNT_; i++)
    {
      int8_t j = 0;
      do
        {
          if(serials[j] == 32)
            {
              j++;
              continue;
            }

          unit[i].serial[j] = serials[j];
          j++;
        }
      while(serials[j] != 44  && j < serialLth);

      serials = serials+j+1;

      status = ps4000aOpenUnit(&unit[i].handle, unit[i].serial);

      if(status == PICO_OK)
        std::cout << "Pico " << unit[i].serial << " check.\n";
      else
        std::cout << "Error: open_unit(): " << std::hex << status << std::endl;
    }
}


void Window::get_unit_info()
{
  for(int i = 0; i < _UNITCOUNT_; i++)
    {
      unit[i].minRange        = PS4000A_10MV;
      unit[i].maxRange        = PS4000A_50V;
      unit[i].channelCount    = PS4000A_MAX_CHANNELS;
      ps4000aMaximumValue(unit[i].handle, &unit[i].maxSampleValue);
      ps4000aMinimumValue(unit[i].handle, &unit[i].minSampleValue);
    }
}


  void Window::set_channels()
  {

    for(int16_t i = 0; i < _UNITCOUNT_; i++)
      {
        for (int ch = 0; ch < unit[i].channelCount; ch++)
          {
            unit[i].channelSettings[ch].range         = (PICO_CONNECT_PROBE_RANGE)PS4000A_5V;
            unit[i].channelSettings[ch].enabled       = true;
            unit[i].channelSettings[ch].bufferEnabled = false;
            unit[i].channelSettings[ch].mode          = OFF;
            unit[i].channelSettings[ch].offset        = 0.0;
            unit[i].channelSettings[ch].maxOffset     = 0.0;
            unit[i].channelSettings[ch].minOffset     = 0.0;
            unit[i].channelSettings[ch].coupling      = (PS4000A_COUPLING)false;
          }
      }
  }


  void Window::set_channels_of_pico()
  {
    for(int16_t i = 0; i < _UNITCOUNT_; i++)
      {
        for(int ch = 0; ch < unit[i].channelCount; ch++)
          {
            PICO_STATUS status = ps4000aSetChannel(unit[i].handle,
                                                   (PS4000A_CHANNEL)(PS4000A_CHANNEL_A + ch),
                                                   unit[i].channelSettings[ch].enabled,
                                                   unit[i].channelSettings[ch].coupling,
                                                   unit[i].channelSettings[ch].range,
                                                   unit[i].channelSettings[ch].offset);

            printf(status?"SetDefaults:ps4000aSetChannel------ 0x%08lx \n":"", (long unsigned int)status);
          }
      }
  }


  void Window::get_allowed_offset()
  {
    for(int16_t i = 0; i < _UNITCOUNT_; i++)
      {
        for(int ch = 0; ch < unit[i].channelCount; ch++)
          {
            ps4000aGetAnalogueOffset(unit[i].handle,
                                     unit[i].channelSettings[ch].range,
                                     unit[i].channelSettings[ch].coupling,
                                     &unit[i].channelSettings[ch].maxOffset,
                                     &unit[i].channelSettings[ch].minOffset);
          }
      }
  }


void Window::set_main_window()
{
  splitter->addWidget(timePlot);
  splitter->addWidget(xyPlot);
  setCentralWidget(splitter);
  timePlot->show();
  xyPlot->show();

  resize(1700, 800);
  addToolBar(toolBar);

  timePlot->axisRect()->setupFullAxesBox(true);
  timePlot->setInteraction(QCP::iRangeZoom, true);
  timePlot->setInteraction(QCP::iRangeDrag, true);
  xyPlot->axisRect()->setupFullAxesBox(true);
  xyPlot->setInteraction(QCP::iRangeZoom, true);
  xyPlot->setInteraction(QCP::iRangeDrag, true);

  timePlot->setSelectionRectMode(QCP::srmCustom);
  connect(timePlot->selectionRect(), &QCPSelectionRect::started, this, &Window::set_rawValue1);
  connect(timePlot->selectionRect(), &QCPSelectionRect::accepted, this, &Window::set_rawValue2);

  colorMap->setGradient(QCPColorGradient::gpGrayscale);
  colorMap->setDataRange(QCPRange(-1.0, 1.0));
  colorMap->data()->setSize(200, 200);
  colorMap->data()->fill(0.0);

  std::vector<std::string> labels = {"X", "Y", "Z0", "Z1", "Z2", "Z3", "Z4", "Z5", "Z6", "Z7", "Z8", "Z9"};
  for(auto p: labels)
    {
      timePlot->addGraph();
    }

  // timePlot->graph(0)->setPen(QPen(QColor(40, 110, 255)));
  QSharedPointer<QCPAxisTickerTime> timeTicker(new QCPAxisTickerTime);
  timeTicker->setTimeFormat("%h:%m:%s");
  timePlot->xAxis->setTicker(timeTicker);
  timePlot->yAxis->setRange(-10.2, 10.2);
}




void Window::set_actions()
{
  streamButton = new QPushButton();
  streamButton->setText(g_streamIsRunning ? "&Stop" : "&Start");
  toolBar->addWidget(streamButton);
  connect(streamButton, SIGNAL(clicked()), this, SLOT(stream_button_slot()));


  saveButton = new QPushButton(tr("&Save"));
  toolBar->addWidget(saveButton);
  connect(saveButton, SIGNAL(clicked()), this, SLOT(save_button_slot()));

  videoButton = new QPushButton(tr("&Video"));
  toolBar->addWidget(videoButton);
  connect(videoButton, SIGNAL(clicked()), this, SLOT(video_button_slot()));


  // scaleOffsetBox = new QDoubleSpinBox();
  // scaleOffsetBox->setMaximum(20);
  // scaleOffsetBox->setMinimum(-20);
  // scaleOffsetBox->setSingleStep(0.1);
  // scaleOffsetBox->setValue(0);
  // scaleOffsetBoxAction = toolBar->addWidget(scaleOffsetBox);

  // scaleAmplitudeBox = new QDoubleSpinBox();
  // scaleAmplitudeBox->setMaximum(20);
  // scaleAmplitudeBox->setMinimum(0);
  // scaleAmplitudeBox->setSingleStep(0.1);
  // scaleAmplitudeBox->setValue(1);
  // scaleAmplitudeBoxAction = toolBar->addWidget(scaleAmplitudeBox);

  sizeBox = new QSpinBox();
  sizeBox->setMaximum(350);
  sizeBox->setMinimum(50);
  sizeBox->setSingleStep(10);
  sizeBox->setValue(200);
  sizeBoxAction = toolBar->addWidget(sizeBox);
  connect(sizeBox, SIGNAL(valueChanged(int)), this, SLOT(set_size_slot(int)));

  show_ChannelMenu = new QAction(tr("&Channel"));
  menuBar()->addAction(show_ChannelMenu);
  connect(show_ChannelMenu, SIGNAL(triggered()), this, SLOT(show_channel_menu_slot()));

  split_screen_Action = new QAction(tr("&Split"));
  connect(split_screen_Action, SIGNAL(triggered()), this, SLOT(split_screen()));

  xyplot_screen_Action = new QAction(tr("&X-Y"));
  connect(xyplot_screen_Action, SIGNAL(triggered()), this, SLOT(xyplot_screen()));

  timeplot_screen_Action = new QAction(tr("&Time"));
  connect(timeplot_screen_Action, SIGNAL(triggered()), this, SLOT(timeplot_screen()));

  view = menuBar()->addMenu(tr("&View"));
  view->addAction(split_screen_Action);
  view->addAction(xyplot_screen_Action);
  view->addAction(timeplot_screen_Action);

  graphs = new QMenu();
  show_channel_list = new QAction(tr("&Pico Channel"));
  show_math_channel_window = new QAction(tr("&Math Channel"));
  ColorMapDataChooser_Action = new QAction(tr("&XY-Signal"));
  graphs = menuBar()->addMenu(tr("&Graphs"));
  graphs->addAction(show_channel_list);
  graphs->addAction(show_math_channel_window);
  graphs->addAction(ColorMapDataChooser_Action);
}


void Window::set_connections()
{
  connect(ChannelWindow_Obj, SIGNAL(do_work(UNIT *)), this, SLOT(stream_button_slot()));
  connect(this, SIGNAL(do_work(UNIT *)), Worker_Obj, SLOT(stream_data(UNIT *)));
  connect(Worker_Obj, SIGNAL(data(std::vector<double>)), this, SLOT(data(std::vector<double>)));
  connect(Worker_Obj, SIGNAL(unit_stopped_signal()), loop, SLOT(quit()));
}



void Window::set_rawValue1(QMouseEvent * event)
{
  rawValueAmplitude1 = timePlot->yAxis->pixelToCoord(event->y());
}


void Window::set_rawValue2(QRect dum, QMouseEvent * event)
{
  rawValueAmplitude2 = timePlot->yAxis->pixelToCoord(event->y());
  calculate_greyscale();
  colorMap->setDataRange(QCPRange(greyScaleOffset-greyScaleAmplitude, greyScaleOffset+greyScaleAmplitude));
  colorMap->data()->fill(greyScaleOffset);
  xyPlot->replot();
}


void Window::calculate_greyscale()
{
  greyScaleAmplitude = rawValueAmplitude1 > rawValueAmplitude2 ? (rawValueAmplitude1 - rawValueAmplitude2)/2.0 : (rawValueAmplitude2 - rawValueAmplitude1)/2.0;
  greyScaleOffset = rawValueAmplitude1 > rawValueAmplitude2 ? rawValueAmplitude2+greyScaleAmplitude : rawValueAmplitude1+greyScaleAmplitude;
}


void Window::show_channel_menu_slot()
{
  ChannelWindow_Obj->show();
}


void Window::set_size_slot(int size)
{
  colorMap->data()->setSize(size, size);
  xyPlot->replot();
}


void Window::split_screen()
{
  resize(1700, 800);
  timePlot->show();
  xyPlot->show();
}


void Window::timeplot_screen()
{
  resize(800, 800);
  xyPlot->hide();
  timePlot->show();
}


void Window::xyplot_screen()
{
  resize(800, 800);
  timePlot->hide();
  xyPlot->show();
}


void Window::stream_button_slot()
{
  if(!g_streamIsRunning)
    {
      emit(do_work(unit));
      g_streamIsRunning = true;
    }
  else if(g_streamIsRunning)
    {
      g_streamIsRunning = false;
      videoIsRunning    = false;
      videoButton->setText("&Video");
    }

  streamButton->setText(g_streamIsRunning ? "&Stop" : "&Start");
}


void Window::save_button_slot()
{
  timePlot->savePng("images/time/" + QString::number(QDateTime::currentMSecsSinceEpoch()) + ".png");
    xyPlot->savePng("images/xy/" + QString::number(QDateTime::currentMSecsSinceEpoch()) + ".png");
}


void Window::video_button_slot()
{
  if(!videoIsRunning)
    {
      frameCounter = 0;

      while(!videoIsRunning &&
            QDir("videos/" + QString::number(videoCounter)).exists())
        videoCounter++;
      QDir().mkdir("videos/" + QString::number(videoCounter));
    }

  videoIsRunning = !videoIsRunning;

  videoButton->setText(videoIsRunning ? "&Running..." : "&Video");

}


void Window::data(std::vector<double> d)
{
  data_vec = d;
  int xInd, yInd;

  for(int i = X; i < Z9+1; i++)
    timePlot->graph(i-1)->addData((double)counter, data_vec[i-1]);

  for(auto e : expression_vec.keys())
    {
      double val = e->value();
      timePlot->graph(expression_vec.value(e))->addData((double)counter, val);
      mathChannel_vec[expression_vec.value(e)] = val;
    }

  colorMap->data()->coordToCell(data_vec[X-1],data_vec[Y-1],&xInd,&yInd);
  colorMap->data()->setCell(xInd, yInd, *colorMapData_ptr);
  counter++;
  if(counter%3000 == 0)
    {
      timePlot->xAxis->setRange(counter, 800, Qt::AlignRight);
      timePlot->replot();
      xyPlot->replot();

      if(videoIsRunning)
        {
          xyPlot->savePng("videos/" + QString::number(videoCounter) + "/" + QString::number(frameCounter) + ".png");
          frameCounter++;
        }
    }
  if(counter%1000000 == 0)
    for(int i = 0; i < timePlot->graphCount(); i++)
      timePlot->graph(i)->data()->clear();
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
  for(int16_t i = 0; i < _UNITCOUNT_; i++)
    ps4000aCloseUnit(unit[i].handle);
  Thread_Obj.quit();
  Thread_Obj.wait();
  QCoreApplication::quit();
  event->accept();
}



GraphWindow::GraphWindow(Window * parent)
  : layout(new QGridLayout)
  , parent(parent)
{
  std::vector<std::string> labels = {"X", "Y", "Z0", "Z1", "Z2", "Z3", "Z4", "Z5", "Z6", "Z7", "Z8", "Z9"};
  for(auto p: labels)
    {
      QCheckBox * ptr = new QCheckBox(tr(p.c_str()));
      layout->addWidget(ptr);
      connect(ptr, SIGNAL(stateChanged(int)), this, SLOT(update_graphs()));
    }

  setLayout(layout);

  connect(parent->show_channel_list, SIGNAL(triggered()), this, SLOT(show()));
}


void GraphWindow::update_graphs()
{
  int rows = layout->rowCount();
  for(int i = 0; i < rows; i++)
    {
      if(dynamic_cast<QCheckBox*>(layout->itemAtPosition(i,0)->widget())->isChecked())
        parent->timePlot->graph(i)->setVisible(true);
      else
        parent->timePlot->graph(i)->setVisible(false);
    }
}



MathWindow::MathWindow(Window * parent)
  : parent(parent)
  , layout(new QGridLayout)
  , entryField(new QLineEdit)
  , eval_button(new QPushButton(tr("&Eval")))
  , map(new QMap<QString, Equation*>)
{
  layout->addWidget(entryField, 0, 0);
  layout->addWidget(eval_button, 0, 1);
  setLayout(layout);

  connect(eval_button, SIGNAL(clicked()), this, SLOT(eval_slot()));
  connect(parent->show_math_channel_window, SIGNAL(triggered()), this, SLOT(show()));
}


void MathWindow::eval_slot()
{
  if(!map->contains(entryField->text()))
    map->insert(entryField->text(), new Equation(entryField->text(), this));
}



Equation::Equation(QString equation_str, MathWindow * parent)
  : parent(parent)
  , equation_str(equation_str)
  , layout(new QGridLayout)
  , box(new QGroupBox(equation_str, this))
  , symbol_table(new exprtk::symbol_table<double>)
  , expression(new exprtk::expression<double>)
  , parser(new exprtk::parser<double>)
  , params(std::vector<double>(1000))
{
  int i = 0;
  QVector<QString>            tracker;
  for(auto c: equation_str)
    {
      QString str(c);

      if(str.contains(QRegExp("[a-m]")) && !tracker.contains(str))
        {
          tracker.push_back(str);

          QSlider * slider = new QSlider(Qt::Vertical, this);
          slider->setMaximum(100);
          slider->setMinimum(-100);
          slider->setSliderPosition(2);
          layout->addWidget(slider, 1, layout->columnCount());
          slider->show();
          connect(slider, &QSlider::valueChanged, [this, slider, equation_str, i, c](){this->params[i] = (double)slider->value();});

          QLabel *  label  = new QLabel(str, this);
          layout->addWidget(label, 0, layout->columnCount());

          if(!symbol_table->add_variable(str.toStdString(), params[i]))
            std::cout << "Error in symbol table\n";
          i++;
        }
    }

  QRegExp rx("[x-z][0-7]");
  int pos = 0;
  QStringList list;
  while(((pos = rx.indexIn(equation_str, pos)) != -1) && !tracker.contains(rx.cap(0)))
    {
      tracker.push_back(rx.cap(0));
      std::string str = rx.cap(0).toStdString().c_str();
      if(str=="x0"){symbol_table->add_variable(str, parent->parent->data_vec[X-1]);}
      if(str=="y0"){symbol_table->add_variable(str, parent->parent->data_vec[Y-1]);}
      if(str=="z0"){symbol_table->add_variable(str, parent->parent->data_vec[Z0-1]);}
      if(str=="z1"){symbol_table->add_variable(str, parent->parent->data_vec[Z1-1]);}
      if(str=="z2"){symbol_table->add_variable(str, parent->parent->data_vec[Z2-1]);}
      if(str=="z3"){symbol_table->add_variable(str, parent->parent->data_vec[Z3-1]);}
      if(str=="z4"){symbol_table->add_variable(str, parent->parent->data_vec[Z4-1]);}
      if(str=="z5"){symbol_table->add_variable(str, parent->parent->data_vec[Z5-1]);}
      if(str=="z6"){symbol_table->add_variable(str, parent->parent->data_vec[Z6-1]);}
      if(str=="z7"){symbol_table->add_variable(str, parent->parent->data_vec[Z7-1]);}
      if(str=="z8"){symbol_table->add_variable(str, parent->parent->data_vec[Z8-1]);}
      if(str=="z9"){symbol_table->add_variable(str, parent->parent->data_vec[Z9-1]);}
      pos += rx.matchedLength();
    }

  expression->register_symbol_table(*symbol_table);
  if(!parser->compile(equation_str.toStdString(), *expression))
    printf("Error: %s\n", parser->error().c_str());

  parent->parent->timePlot->addGraph();
  parent->parent->expression_vec.insert(expression, parent->parent->timePlot->graphCount()-1);
  parent->parent->ColorMapDataChooser_Obj->expression_vec.insert(expression, equation_str);
  parent->parent->ColorMapDataChooser_Obj->update_buttons();

  box->setLayout(layout);
  parent->layout->addWidget(box);
}



ColorMapDataChooser::ColorMapDataChooser(Window * parent)
  : parent(parent)
  , layout(new QVBoxLayout)
{
  std::vector<std::string> labels = {"X", "Y", "Z0", "Z1", "Z2", "Z3", "Z4", "Z5", "Z6", "Z7", "Z8", "Z9"};
  for(auto p: labels)
    {
      QRadioButton * ptr = new QRadioButton(tr(p.c_str()), this);
      layout->addWidget(ptr);
      connect(ptr, &QRadioButton::toggled, this, &ColorMapDataChooser::check_buttons_channel);
    }
  setLayout(layout);
  connect(parent->ColorMapDataChooser_Action, SIGNAL(triggered()), this, SLOT(show()));
}


void ColorMapDataChooser::check_buttons_channel()
{
  for(int i = X; i < Z9+1; i++)
    if(((QRadioButton*)layout->itemAt(i-1)->widget())->isChecked())
      parent->colorMapData_ptr = (double*)&parent->data_vec[i-1];
}


void ColorMapDataChooser::check_buttons_math()
{
  for(int i = Z9; i < expression_vec.size()+Z9; i++)
    if(((QRadioButton*)layout->itemAt(i)->widget())->isChecked())
      parent->colorMapData_ptr = &parent->mathChannel_vec[i];
}


void ColorMapDataChooser::update_buttons()
{
  exprtk::expression<double>* e = expression_vec.lastKey();
  QRadioButton * ptr = new QRadioButton(tr(expression_vec.value(e).toStdString().c_str()));
  layout->addWidget(ptr);
  connect(ptr, &QRadioButton::toggled, this, &ColorMapDataChooser::check_buttons_math);
}
