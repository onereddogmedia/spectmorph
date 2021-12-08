// Licensed GNU LGPL v2.1 or later: http://www.gnu.org/licenses/lgpl-2.1.html

#ifndef SPECTMORPH_EVENT_LOOP_HH
#define SPECTMORPH_EVENT_LOOP_HH

#include "smdrawutils.hh"

namespace SpectMorph
{

class EventLoop : public SignalReceiver
{
  std::vector<Window *> windows;
  std::vector<Widget *> delete_later_widgets;
  int                   m_level = 0;

public:
  void wait_event_fps();
  void process_events();
  int  level() const;

  void add_window (Window *window);
  void remove_window (Window *window);
  bool window_alive (Window *window) const;

  void add_delete_later (Widget *w);
  void on_widget_deleted (Widget *w);

  Signal<> signal_before_process;
};

}

#endif
