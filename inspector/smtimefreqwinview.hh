// Licensed GNU LGPL v2.1 or later: http://www.gnu.org/licenses/lgpl-2.1.html

#ifndef SPECTMORPH_TIME_FREQ_WIN_VIEW_HH
#define SPECTMORPH_TIME_FREQ_WIN_VIEW_HH

#include "smtimefreqview.hh"
#include "smzoomcontroller.hh"
#include "smspectrumwindow.hh"
#include "smfftparamwindow.hh"
#include "smsamplewindow.hh"

#include <QScrollArea>

namespace SpectMorph {

class Navigator;
class TimeFreqWinView : public QWidget
{
  Q_OBJECT

  QLabel             *position_label;
  QSlider            *position_slider;
  QLabel             *min_db_label;
  QSlider            *min_db_slider;
  QLabel             *boost_label;
  QSlider            *boost_slider;
  QScrollArea        *scroll_area;
  TimeFreqView       *m_time_freq_view;
  ZoomController     *zoom_controller;

  Navigator          *navigator;

public:
  TimeFreqWinView (Navigator *navigator);

  TimeFreqView *time_freq_view();

public slots:
  void on_wav_data_changed();
  void on_zoom_changed();
  void on_position_changed();
  void on_display_params_changed();
  void on_progress_changed();
  void on_analysis_changed();
  void on_frequency_grid_changed();
};

}

#endif
