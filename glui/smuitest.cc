// Licensed GNU LGPL v2.1 or later: http://www.gnu.org/licenses/lgpl-2.1.html

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
#include "smmenubar.hh"
#include "smrandom.hh"
#include "smfixedgrid.hh"
#include "smcheckbox.hh"
#include "smbutton.hh"
#include "smtimer.hh"
#include "smled.hh"
#include "smshortcut.hh"
#include "smaboutdialog.hh"
#include "smeventloop.hh"
#include "smprogressbar.hh"

using namespace SpectMorph;

using std::vector;
using std::string;

class ColorTest : public Widget
{
public:
  Color color;
  ColorTest (Widget *parent, Color color) :
    Widget (parent),
    color (color)
  {
  }
  void draw (const DrawEvent& devent) override
  {
    cairo_t *cr = devent.cr;
    cairo_rectangle (cr, 0, 0, width(), height());
    cairo_set_source_rgb (cr, color.red(), color.green(), color.blue());
    cairo_fill (cr);
  }
};

class MainWindow : public Window
{
public:
  MainWindow (EventLoop& event_loop, int width, int height, PuglNativeWindow win_id = 0, bool resize = true) :
    Window (event_loop, "SpectMorph UI Test", width, height, win_id, resize)
  {
    vector<string> sl_params { "Skip", "Attack", "Sustain", "Decay", "Release" };
    FixedGrid grid;

    MenuBar *menu_bar = new MenuBar (this);
    Menu *file_menu = menu_bar->add_menu ("File");
    Menu *preset_menu = menu_bar->add_menu ("Open Preset");
    Menu *op_menu = menu_bar->add_menu ("Add Operator");
    Menu *help_menu = menu_bar->add_menu ("Help");
    Label *menu_label = new Label (this, "Menu Label");

    auto set_items = [menu_label, this](Menu *m, const std::vector<string>& items) {
      for (auto i : items) {
        MenuItem *item = m->add_item (i);
        connect (item->signal_clicked, [=]()
          {
            printf ("menu item %s selected\n", i.c_str());
            menu_label->set_text ("selected: " + i);
          });
      }
    };
    set_items (file_menu, {"Import...", "Export...", "Load Instrument Set..."});
    set_items (preset_menu, {"2x2 Grid Morph using GUI", "Fancy Preset", "Cool Preset" });
    set_items (op_menu, {"Source", "Output", "Linear Morph", "Grid Morph", "LFO" });
    set_items (help_menu, {"About..."});
    grid.add_widget (menu_bar, 1, 1, 46, 3);

    grid.dx = 0;
    grid.dy = 4;

    grid.add_widget (menu_label, 1, 31, 46, 5);

    grid.add_widget (new Frame (this), 1, 1, 43, sl_params.size() * 2 + 15);

    Label *op_title = new Label (this, "Output: Output #1");
    op_title->set_align (TextAlign::CENTER);
    op_title->set_bold (true);
    grid.add_widget (op_title, 1, 1, 43, 4);

    ComboBox *cb1 = new ComboBox (this);
    ComboBox *cb2 = new ComboBox (this);

    grid.add_widget (new Label (this, "LSource"), 3, 5, 7, 3);
    grid.add_widget (cb1, 10, 5, 32, 3);
    grid.add_widget (new Label (this, "RSource"), 3, 8, 7, 3);
    grid.add_widget (cb2, 10, 8, 32, 3);

    grid.add_widget (new ScrollBar (this, 0.3, Orientation::VERTICAL), 45, 1, 2, 42);

    vector<string> item_vec = {
      "*Wind",
      "Alto Flute",
      "Oboe",
      "Bassoon",
      "*Brass",
      "Trumpet",
      "Bass Trombone",
      "French Horn",
      "*Voices",
      "Mirko Ah",
      "Mikro Oh",
      "*Keys",
      "Reed Organ",
      "Synth Saw",
      "*Strings",
      "Violin", "Viola", "Cello", "Double Bass"
    };
    for (auto item : item_vec)
      {
        if (item[0] == '*')
          cb1->add_item (ComboBoxItem (item.c_str() + 1, true));
        else
          cb1->add_item (ComboBoxItem (item));
      }
    for (size_t i = 0; i < 32; i++)
      cb2->add_item (ComboBoxItem ("Some Instrument #" + std::to_string (i)));

    int yoffset = 11;
    Random rng;
    for (auto s : sl_params)
      {
        Label *label = new Label (this, s);
        Slider *slider = new Slider (this, rng.random_double_range (0.0, 1.0));
        Label *value_label = new Label (this, "50%");

        grid.add_widget (label, 3, yoffset, 7, 2);
        grid.add_widget (slider,  10, yoffset, 27, 2);
        grid.add_widget (value_label, 38, yoffset, 5, 2);
        yoffset += 2;

        auto call_back = [=](float value) { value_label->set_text (std::to_string((int) (value * 100 + 0.5)) + "%"); };
        connect (slider->signal_value_changed, call_back);
        call_back (slider->value());
      }
    CheckBox *box1 = new CheckBox (this, "Enable Sine Synthesis");
    grid.add_widget (box1, 3, yoffset, 30, 2);
    yoffset += 2;
    CheckBox *box2 = new CheckBox (this, "Enable Noise Synthesis");
    grid.add_widget (box2, 3, yoffset, 30, 2);

    if (0) // TEXT ALIGN
      {
        vector<string> texts = { "A", "b", "c", "D", ".", "'", "|" };
        for (size_t x = 0; x < texts.size(); x++)
          grid.add_widget (new Label (this, texts[x]), 3 + x * 2, 20, 2, 2);
      }
    Color green (0.1, 0.7, 0.1);
    ColorTest *ct0 = new ColorTest (this, green);
    grid.add_widget (ct0, 3, 40, 2, 2);
    double h, s, v;
    green.get_hsv (&h, &s, &v);
    Color green2;
    green2.set_hsv (h, s, v);
    ColorTest *ct1 = new ColorTest (this, green2);
    grid.add_widget (ct1, 6, 40, 2, 2);
    grid.add_widget (new ColorTest (this, green.lighter (150)), 9, 40, 2, 2);
    grid.add_widget (new ColorTest (this, green.lighter (200)), 12, 40, 2, 2);
    grid.add_widget (new ColorTest (this, green.lighter (250)), 15, 40, 2, 2);
    grid.add_widget (new ColorTest (this, green.darker (150)), 18, 40, 2, 2);
    grid.add_widget (new ColorTest (this, green.darker (200)), 21, 40, 2, 2);
    grid.add_widget (new ColorTest (this, green.darker (250)), 24, 40, 2, 2);

    FileDialogFormats formats ("SpectMorph Preset files", "*.smplan");
    Button *open_button = new Button (this, "Open File");
    connect (open_button->signal_clicked, [=] () { open_file_dialog ("Select SpectMorph Preset", formats, [=](string filename) { printf ("open file: '%s'\n", filename.c_str()); }); });
    grid.add_widget (open_button, 3, 35, 10, 3);

    Button *save_button = new Button (this, "Save File");
    connect (save_button->signal_clicked, [=] () { save_file_dialog ("Select SpectMorph Preset", formats, [=](string filename) { printf ("save file: '%s'\n", filename.c_str()); }); });
    grid.add_widget (save_button, 15, 35, 10, 3);

    Led *timer_led = new Led (this, true);
    grid.add_widget (timer_led, 30, 40, 3, 3);

    ProgressBar *pbar = new ProgressBar (this);
    grid.add_widget (pbar, 1, 29, 43, 3);
    pbar->set_value (-1);

    Slider *progress_slider = new Slider (this, 0);
    grid.add_widget (progress_slider, 1, 26, 43, 3);
    connect (progress_slider->signal_value_changed, [=] (double v) { pbar->set_value (v); });

    Timer *timer = new Timer (this);
    connect (timer->signal_timeout, [=] { timer_led->set_on (!timer_led->on()); });

    Button *toggle_timer = new Button (this, "Timer");
    connect (toggle_timer->signal_clicked, [=] () { if (timer->active()) timer->stop(); else timer->start (500); });
    grid.add_widget (toggle_timer, 27, 35, 10, 3);

    Shortcut::test (this);
  }
};

using std::vector;

int
main (int argc, char **argv)
{
  Main main (&argc, &argv);

  bool quit = false;

  EventLoop event_loop;
  MainWindow window (event_loop, 384, 384);

  window.show();
  window.set_close_callback ([&]() { quit = true; });

  while (!quit) {
    event_loop.wait_event_fps();
    event_loop.process_events();
  }
}
