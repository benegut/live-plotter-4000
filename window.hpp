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


inline int16_t    _UNITCOUNT_;

inline bool       g_streamIsRunning;
inline int32_t    g_sampleCount;
inline uint32_t   g_startIndex;
inline bool       g_ready;


typedef enum
  {
    OFF, X, Y, Z0, Z1, Z2, Z3, Z4, Z5, Z6, Z7, F0, F1
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
  void                      data(double, double,
                                 double, double,
                                 double, double,
                                 double, double,
                                 double, double);
};




class TimePlot : public QCustomPlot
{
};





class XYPlot : public QCustomPlot
{
};



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






class Window : public QMainWindow
{
  Q_OBJECT

public:
  Window();
  void                    start();


private:
  Worker *                Worker_Obj;
  QThread                 Thread_Obj;
  QEventLoop *            loop;

  QSplitter *             splitter;
  QCustomPlot *           timePlot;
  QCustomPlot *           xyPlot;
  QCPColorMap *           colorMap;
  QToolBar *              toolBar;

  UNIT *                  unit;

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
  void                    add_graph();

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
  void                    data(double, double,
                               double, double,
                               double, double,
                               double, double,
                               double, double);
  void                    data_debug(double, double,
                                     double, double,
                                     double, double,
                                     double, double,
                                     double, double);

protected:
  void contextMenuEvent(QContextMenuEvent *event) override;
};



#endif //WINDOW_H
