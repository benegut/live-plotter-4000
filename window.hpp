#ifndef WINDOW_H
#define WINDOW_H

#include <QMainWindow>
#include <QSharedPointer>
#include <QObject>
#include "qcustomplot.h"
#include <QToolBar>

#include <libps4000a-1.0/ps4000aApi.h>
#ifndef PICO_STATUS
#include <libps4000a-1.0/PicoStatus.h>
#endif //PICO_STATUS

#include <string>
#include <vector>
#include "exprtk.hpp"



inline int16_t    _UNITCOUNT_;

inline bool       g_streamIsRunning;
inline int32_t    g_sampleCount;
inline uint32_t   g_startIndex;
inline bool       g_ready;


typedef enum
  {
    OFF, X, Y, Z0, Z1, Z2, Z3, Z4, Z5, Z6, Z7, Z8, Z9
  }MODE;


typedef struct
{
  PICO_CONNECT_PROBE_RANGE  range;
  bool                      enabled;
  bool                      bufferEnabled;
  int16_t *                 driver_buffer;
  int16_t *                 app_buffer;
  MODE                      mode;
  float                     offset;
  float                     maxOffset;
  float                     minOffset;
  PS4000A_COUPLING          coupling;
}CHANNEL_SETTINGS;





typedef struct t_unit
{
  int16_t                   handle;
  int8_t                    serial[10];
  PS4000A_RANGE             minRange;
  PS4000A_RANGE             maxRange;
  int16_t                   channelCount;
  int16_t                   maxSampleValue;
  int16_t                   minSampleValue;
  CHANNEL_SETTINGS          channelSettings[PS4000A_MAX_CHANNELS];
}UNIT;


Q_DECLARE_METATYPE(UNIT);


typedef struct
{
  UNIT *              unit;
}BUFFER_INFO;


struct RangeBox : public QComboBox
{
  RangeBox();
  QGridLayout *               layout;
  std::vector<std::string>    labels;
};



struct TypeBox : public QComboBox
{
  TypeBox();
  QGridLayout *               layout;
  std::vector<std::string>    labels;
};




class Worker : public QObject
{
  Q_OBJECT

public:
                            Worker();
  static void               callback(int16_t, int32_t,
                                     uint32_t, int16_t,
                                     uint32_t, int16_t,
                                     int16_t, void *);

private:
  double                    adc_to_voltage(int, int16_t, int16_t);
  std::vector<double>       voltages;

public slots:
  void                      stream_data(UNIT *);

signals:
  void                      unit_stopped_signal();

  void                      data(std::vector<double>);
};



class Window;


class ChannelWindow : public QWidget
{
  Q_OBJECT

public:
  ChannelWindow(UNIT *, Worker *, QCustomPlot *, QCustomPlot *, QCPColorMap *);

private:
  QGroupBox *                   create_group_box(int,int);
  void                          update_axis();

  QGridLayout *                 layout;
  QGroupBox *                   unit_Box;
  QGridLayout *                 unit_Layout;
  QGroupBox **                  channelBox;
  RangeBox **                   RangeBox_Obj;
  QDoubleSpinBox **             Offset_SpinBox_Obj;
  TypeBox **                    TypeBox_Obj;
  QPushButton *                 Update_Button;

  UNIT *                        unit;
  Worker *                      worker;
  QCustomPlot *                 timePlot;
  QCustomPlot *                 xyPlot;
  QCPColorMap *                 colorMap;
  QEventLoop *                  loop;
  std::vector<double>           voltages;

public slots:
  void                          set_channels();
  void                          set_channels_of_pico();
  void                          get_offset_bounds(int,int);

signals:
  void                          do_work(UNIT *);
};


class GraphWindow : public QWidget
{
  Q_OBJECT

public:
  GraphWindow(Window *);
  QGridLayout *                 layout;

private:
  Window *                parent;

public slots:
  void                    update_graphs();
};


class MathWindow;




class Equation : public QWidget
{
  Q_OBJECT

public:
  Equation(QString, MathWindow *);
  QGridLayout * layout;
  QGroupBox *   box;
  exprtk::symbol_table<double>  * symbol_table;
  exprtk::expression<double>    * expression;
  exprtk::parser<double>        * parser;


private:
  QString                     equation_str;
  MathWindow *                parent;
  std::vector<double>         params;

};



class MathWindow : public QWidget
{
  Q_OBJECT

  public:
  MathWindow(Window *);
  Window *                    parent;
  QGridLayout *               layout;
  QLineEdit *                 entryField;
  QPushButton *               eval_button;
  QMap<QString, Equation*> *  map;

public slots:
  void                    eval_slot();
};



class ColorMapDataChooser : public QWidget
{
  Q_OBJECT

public:
  ColorMapDataChooser(Window *);
  QVBoxLayout *                             layout;
  QMap<QString, int>                        channel_map;
  QMap<exprtk::expression<double>*, QString>    expression_vec;


private:
  Window *            parent;

public slots:
  void                                      check_buttons_channel();
  void                                      update_buttons();
  void                                      check_buttons_math();
};



class Window : public QMainWindow
{
  Q_OBJECT

public:
  Window();
  void                    start();


public:
  Worker *                Worker_Obj;
  QThread                 Thread_Obj;
  QEventLoop *            loop;

  QSplitter *             splitter;
  QCustomPlot *           timePlot;
  QCustomPlot *           xyPlot;
  QCPColorMap *           colorMap;
  QToolBar *              toolBar;

  UNIT *                  unit;

  std::vector<double>     data_vec;

  QPushButton *           streamButton;
  QPushButton *           saveButton;
  QPushButton *           videoButton;
  QDoubleSpinBox *        scaleOffsetBox;
  QAction *               scaleOffsetBoxAction;
  QDoubleSpinBox *        scaleAmplitudeBox;
  QAction *               scaleAmplitudeBoxAction;
  double                  rawValueAmplitude1;
  double                  rawValueAmplitude2; 
  double                  greyScaleAmplitude;
  double                  greyScaleOffset;
  QSpinBox *              sizeBox;
  QAction*                sizeBoxAction;

  ChannelWindow *         ChannelWindow_Obj;
  QAction *               show_ChannelMenu;
  QMenu *                 view;
  QAction *               split_screen_Action;
  QAction *               timeplot_screen_Action;
  QAction *               xyplot_screen_Action;
  QMenu *                 graphs;

  GraphWindow *           GraphWindow_Obj;
  MathWindow *            MathWindow_Obj;

  QMap<exprtk::expression<double>*, int>   expression_vec;

  double *                colorMapData_ptr;
  QVector<double>         mathChannel_vec;

  ColorMapDataChooser *   ColorMapDataChooser_Obj;
  QAction *               ColorMapDataChooser_Action;

public:
  QAction *               show_channel_list;
  QAction *               show_math_channel_window;


  int                     counter;
  bool                    videoIsRunning;
  int                     videoCounter;
  int                     frameCounter;

  void                    open_unit();
  void                    get_unit_info();
  void                    set_channels();
  void                    set_channels_of_pico();
  void                    get_allowed_offset();

  void                    set_main_window();
  void                    set_actions();
  void                    set_connections();

  void                    calculate_greyscale();

  void                    closeEvent(QCloseEvent *);

signals:
  void                    do_work(UNIT *);

public slots:
  void                    set_rawValue1(QMouseEvent *);
  void                    set_rawValue2(QRect, QMouseEvent *);
  void                    show_channel_menu_slot();
  void                    set_size_slot(int);
  void                    split_screen();
  void                    timeplot_screen();
  void                    xyplot_screen();
  void                    stream_button_slot();
  void                    save_button_slot();
  void                    video_button_slot();
  void                    data(std::vector<double>);

protected:
  void contextMenuEvent(QContextMenuEvent *event) override;
};



#endif //WINDOW_H
