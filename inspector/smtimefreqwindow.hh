// Licensed GNU LGPL v2.1 or later: http://www.gnu.org/licenses/lgpl-2.1.html

#ifndef SPECTMORPH_TIME_FREQ_WINDOW_HH
#define SPECTMORPH_TIME_FREQ_WINDOW_HH

#include <QMainWindow>

#include "smtimefreqwinview.hh"
#include "smtimefreqview.hh"

namespace SpectMorph {

class Navigator;
class TimeFreqWinView;
class TimeFreqWindow : public QMainWindow
{
  Q_OBJECT

  TimeFreqWinView *time_freq_win_view;

public:
  TimeFreqWindow (Navigator *navigator);

  TimeFreqView *time_freq_view();
};

}

#endif
