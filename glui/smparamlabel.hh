// Licensed GNU LGPL v2.1 or later: http://www.gnu.org/licenses/lgpl-2.1.html

#ifndef SPECTMORPH_PARAM_LABEL_HH
#define SPECTMORPH_PARAM_LABEL_HH

#include "smlineedit.hh"
#include "smlabel.hh"
#include "smwindow.hh"

namespace SpectMorph
{

class ParamLabelModel
{
public:
  ParamLabelModel();
  virtual ~ParamLabelModel();

  virtual std::string value_text() = 0;
  virtual std::string display_text() = 0;
  virtual void        set_value_text (const std::string& t) = 0;

  Signal<> signal_update_display;
};

class ParamLabelModelDouble : public ParamLabelModel
{
  double value;
  double min_value;
  double max_value;

  std::string value_fmt;
  std::string display_fmt;

public:
  ParamLabelModelDouble (double start, double min_val, double max_val, const std::string& value_fmt, const std::string& display_fmt) :
    value (start),
    min_value (min_val),
    max_value (max_val),
    value_fmt (value_fmt),
    display_fmt (display_fmt)
  {
  }

  std::string
  value_text()
  {
    return string_locale_printf (value_fmt.c_str(), value);
  }
  std::string
  display_text()
  {
    return string_locale_printf (display_fmt.c_str(), value);
  }
  void
  set_value_text (const std::string& t)
  {
    value = sm_atof_any (t.c_str());

    value = sm_bound (min_value, value, max_value);

    signal_value_changed (value);
  }
  void
  set_value (double v)
  {
    value = sm_bound (min_value, v, max_value);

    signal_update_display();
  }
  Signal<double> signal_value_changed;
};

class ParamLabelModelInt : public ParamLabelModel
{
  int value;
  int min_value;
  int max_value;

  std::string display_fmt;

public:
  ParamLabelModelInt (int i, int min_value, int max_value, const char *format = "%d") :
    value (i),
    min_value (min_value),
    max_value (max_value),
    display_fmt (format)
  {
  }

  std::string
  value_text()
  {
    return string_locale_printf ("%d", value);
  }
  std::string
  display_text()
  {
    return string_locale_printf (display_fmt.c_str(), value);
  }
  void
  set_value_text (const std::string& t)
  {
    value = atoi (t.c_str());

    value = sm_bound (min_value, value, max_value);

    signal_value_changed (value);
  }
  void
  set_value (int i)
  {
    value = sm_bound (min_value, i, max_value);

    signal_update_display();
  }
  Signal<int> signal_value_changed;
};

class ParamLabelModelString : public ParamLabelModel
{
  std::string value;

public:
  ParamLabelModelString (const std::string& s):
    value (s)
  {
  }
  std::string
  value_text()
  {
    return value;
  }
  std::string
  display_text()
  {
    return value;
  }
  void
  set_value_text (const std::string& t)
  {
    value = t;

    signal_value_changed (value);
  }
  Signal<std::string> signal_value_changed;
};

class ParamLabel : public Label
{
  bool      pressed = false;
  LineEdit *line_edit = nullptr;

  std::unique_ptr<ParamLabelModel> model;
public:
  ParamLabel (Widget *parent, ParamLabelModel *model) :
    Label (parent, ""),
    model (model)
  {
    connect (model->signal_update_display, this, &ParamLabel::on_update_display);

    on_update_display();
  }
  void
  mouse_press (const MouseEvent& event) override
  {
    if (event.button == LEFT_BUTTON)
      pressed = true;
  }
  void
  mouse_release (const MouseEvent& event) override
  {
    if (event.button != LEFT_BUTTON || !pressed)
      return;
    pressed = false;

    if (!line_edit)
      {
        line_edit = new LineEdit (this, model->value_text());
        line_edit->select_all();
        line_edit->set_height (height());
        line_edit->set_width (width());
        line_edit->set_x (0);
        line_edit->set_y (0);

        connect (line_edit->signal_return_pressed, this, &ParamLabel::on_return_pressed);
        connect (line_edit->signal_focus_out, this, &ParamLabel::on_return_pressed);

        window()->set_keyboard_focus (line_edit, true);

        set_text ("");
      }
  }
  void
  on_return_pressed()
  {
    if (!line_edit->visible())
      return;

    model->set_value_text (line_edit->text());
    set_text (model->display_text());
    line_edit->delete_later();
    line_edit = nullptr;
  }
  void
  on_update_display()
  {
    set_text (model->display_text());
  }
};

}

#endif
