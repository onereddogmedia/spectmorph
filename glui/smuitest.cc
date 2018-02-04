#include <stdio.h>
#include "pugl/gl.h"
#if !__APPLE__
#include "GL/glext.h"
#endif
#include "pugl/pugl.h"
#include <unistd.h>
#include <math.h>
#include <string>
#include <vector>
#include <functional>

#include "smwidget.hh"
#include "smlabel.hh"
#include "smslider.hh"
#include "smwindow.hh"
#include "smmain.hh"
#include "smframe.hh"
#include "smcombobox.hh"
#include "smscrollbar.hh"
#include "smrandom.hh"

using namespace SpectMorph;

using std::vector;
using std::string;

struct FixedGrid
{
  void add_widget (Widget *w, double x, double y, double width, double height)
  {
    w->x = x * 8;
    w->y = y * 8;
    w->width = width * 8;
    w->height = height * 8;
  }
};

class MainWindow : public Window
{
public:
  MainWindow (int width, int height, PuglNativeWindow win_id = 0) :
    Window (width, height, win_id, true)
  {
    Label *mw_label = new Label (this, 0, 200, 400, 200, " --- main window --- ");
    mw_label->align = Label::Align::CENTER;

    vector<string> sl_params { "Skip", "Attack", "Sustain", "Decay", "Release" };
    FixedGrid grid;

    grid.add_widget (new Frame (this, 0, 0, 0, 0), 1, 1, 43, sl_params.size() * 2 + 11);

    Label *op_title = new Label (this, 0, 0, 0, 0, "Output: Output #1");
    op_title->align = Label::Align::CENTER;
    grid.add_widget (op_title, 1, 1, 43, 4);

    ComboBox *cb1 = new ComboBox (this);
    ComboBox *cb2 = new ComboBox (this);

    grid.add_widget (new Label (this, 0, 0, 0, 0, "LSource"), 3, 5, 7, 3);
    grid.add_widget (cb1, 10, 5, 32, 3);
    grid.add_widget (new Label (this, 0, 0, 0, 0, "RSource"), 3, 8, 7, 3);
    grid.add_widget (cb2, 10, 8, 32, 3);

    grid.add_widget (new ScrollBar (this, 0.3), 45, 1, 2, 30);

    cb1->items = { "Trumpet", "Bass Trombone", "French Horn", "Violin", "Cello" };
    for (size_t i = 0; i < 32; i++)
      cb2->items.push_back ("Some Instrument #" + std::to_string (i));

    int yoffset = 11;
    Random rng;
    for (auto s : sl_params)
      {
        Label *label = new Label (this, 0, 0, 0, 0, s);
        Slider *slider = new Slider (this, 0, 0, 0, 0, rng.random_double_range (0.0, 1.0));
        Label *value_label = new Label (this, 0, 0, 0, 0, "50%");

        grid.add_widget (label, 3, yoffset, 7, 2);
        grid.add_widget (slider,  10, yoffset, 27, 2);
        grid.add_widget (value_label, 38, yoffset, 5, 2);
        yoffset += 2;

        auto call_back = [=](float value) { value_label->text = std::to_string((int) (value * 100 + 0.5)) + "%"; };
        slider->set_callback (call_back);
        call_back (slider->value);
      }

    if (0) // TEXT ALIGN
      {
        vector<string> texts = { "A", "b", "c", "D", ".", "'", "|" };
        for (size_t x = 0; x < texts.size(); x++)
          grid.add_widget (new Label (this, 0, 0, 0, 0, texts[x]), 3 + x * 2, 20, 2, 2);
      }
  }
};

using std::vector;

#if !VST_PLUGIN
int
main (int argc, char **argv)
{
  sm_init (&argc, &argv);

  MainWindow window (384, 384);

  window.show();

  while (!window.quit) {
    window.wait_for_event();
    window.process_events();
  }
}
#endif
