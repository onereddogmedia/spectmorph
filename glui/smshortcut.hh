// Licensed GNU LGPL v2.1 or later: http://www.gnu.org/licenses/lgpl-2.1.html

#ifndef SPECTMORPH_SHORTCUT_HH
#define SPECTMORPH_SHORTCUT_HH

#include "smwindow.hh"

namespace SpectMorph
{

class Shortcut
{
  Window *window     = nullptr;
  PuglMod mod        = PuglMod (0);
  bool    mod_check  = false;
  uint32_t character = 0;

public:
  Shortcut (Window *window, PuglMod mod, uint32_t character);
  Shortcut (Window *window, uint32_t character);

  ~Shortcut();

  bool focus_override();
  bool key_press_event (const PuglEventKey& key_event);

  static void test (Window *window);

  Signal<> signal_activated;
};

}

#endif
