// Licensed GNU LGPL v3 or later: http://www.gnu.org/licenses/lgpl.html

#include "smmorphlinearview.hh"
#include "smmorphplan.hh"
#include "smcomboboxoperator.hh"
#include <birnet/birnet.hh>

#include <QLabel>
#include <QCheckBox>

using namespace SpectMorph;

using std::string;
using std::vector;

#define CONTROL_TEXT_GUI "Gui Slider"
#define CONTROL_TEXT_1   "Control Signal #1"
#define CONTROL_TEXT_2   "Control Signal #2"

MorphLinearView::MorphLinearView (MorphLinear *morph_linear, MorphPlanWindow *morph_plan_window) :
  MorphOperatorView (morph_linear, morph_plan_window),
  morph_linear (morph_linear),
  operator_filter (morph_linear, MorphOperator::OUTPUT_AUDIO),
  control_operator_filter (morph_linear, MorphOperator::OUTPUT_CONTROL)
{
  QGridLayout *grid_layout = new QGridLayout();

  // LEFT SOURCE
  grid_layout->addWidget (new QLabel ("Left Source"), 0, 0);

  left_combobox = new ComboBoxOperator (morph_linear->morph_plan(), &operator_filter);
  left_combobox->set_active (morph_linear->left_op());
  grid_layout->addWidget (left_combobox, 0, 1);
  connect (left_combobox, SIGNAL (active_changed()), this, SLOT (on_operator_changed()));

  // RIGHT SOURCE
  grid_layout->addWidget (new QLabel ("Right Source"), 1, 0);
  right_combobox = new ComboBoxOperator (morph_linear->morph_plan(), &operator_filter);
  right_combobox->set_active (morph_linear->right_op());
  grid_layout->addWidget (right_combobox, 1, 1);
  connect (right_combobox, SIGNAL (active_changed()), this, SLOT (on_operator_changed()));

  // CONTROL INPUT
  grid_layout->addWidget (new QLabel ("Control Input"), 2, 0);

  control_combobox = new ComboBoxOperator (morph_linear->morph_plan(), &control_operator_filter);
  control_combobox->add_str_choice (CONTROL_TEXT_GUI);
  control_combobox->add_str_choice (CONTROL_TEXT_1);
  control_combobox->add_str_choice (CONTROL_TEXT_2);
  control_combobox->set_none_ok (false);

  grid_layout->addWidget (control_combobox, 2, 1);

  connect (control_combobox, SIGNAL (active_changed()), this, SLOT (on_control_changed()));

  if (morph_linear->control_type() == MorphLinear::CONTROL_GUI) /* restore value */
    control_combobox->set_active_str_choice (CONTROL_TEXT_GUI);
  else if (morph_linear->control_type() == MorphLinear::CONTROL_SIGNAL_1)
    control_combobox->set_active_str_choice (CONTROL_TEXT_1);
  else if (morph_linear->control_type() == MorphLinear::CONTROL_SIGNAL_2)
    control_combobox->set_active_str_choice (CONTROL_TEXT_2);
  else if (morph_linear->control_type() == MorphLinear::CONTROL_OP)
    control_combobox->set_active (morph_linear->control_op());
  else
    {
      assert (false);
    }

  // MORPHING
  grid_layout->addWidget (new QLabel ("Morphing"), 3, 0);
  morphing_stack = new QStackedWidget();

  morphing_slider = new QSlider (Qt::Horizontal);
  morphing_slider->setRange (-100, 100);

  connect (morphing_slider, SIGNAL (valueChanged(int)), this, SLOT (on_morphing_changed(int)));

  morphing_label = new QLabel();

  QWidget *morphing_hbox_widget = new QWidget();
  QHBoxLayout *morphing_hbox = new QHBoxLayout();
  morphing_hbox->setContentsMargins (0, 0, 0, 0);
  morphing_hbox->addWidget (morphing_slider);
  morphing_hbox->addWidget (morphing_label);
  morphing_hbox_widget->setLayout (morphing_hbox);
  morphing_stack->addWidget (morphing_hbox_widget);
  morphing_stack->addWidget (new QLabel ("from control input"));
  grid_layout->addWidget (morphing_stack, 3, 1);

  int morphing_slider_value = lrint (morph_linear->morphing() * 100); /* restore value from operator */
  morphing_slider->setValue (morphing_slider_value);
  on_morphing_changed (morphing_slider_value);

  // FLAG: DB LINEAR
  QCheckBox *db_linear_box = new QCheckBox ("dB Linear Morphing");
  db_linear_box->setChecked (morph_linear->db_linear());
  grid_layout->addWidget (db_linear_box, 4, 0, 1, 2);
  connect (db_linear_box, SIGNAL (toggled (bool)), this, SLOT (on_db_linear_changed (bool)));

  // FLAG: USE LPC
  QCheckBox *use_lpc_box = new QCheckBox ("Use LPC Envelope");
  use_lpc_box->setChecked (morph_linear->use_lpc());
  grid_layout->addWidget (use_lpc_box, 5, 0, 1, 2);
  connect (use_lpc_box, SIGNAL (toggled (bool)), this, SLOT (on_use_lpc_changed (bool)));

  update_slider();

  setLayout (grid_layout);
}

void
MorphLinearView::on_morphing_changed (int new_value)
{
  double dvalue = new_value * 0.01;
  morphing_label->setText (Birnet::string_printf ("%.2f", dvalue).c_str());
  morph_linear->set_morphing (dvalue);
}

void
MorphLinearView::on_control_changed()
{
  MorphOperator *op = control_combobox->active();
  if (op)
    {
      morph_linear->set_control_op (op);
      morph_linear->set_control_type (MorphLinear::CONTROL_OP);
    }
  else
    {
      string text = control_combobox->active_str_choice();

      if (text == CONTROL_TEXT_GUI)
        morph_linear->set_control_type (MorphLinear::CONTROL_GUI);
      else if (text == CONTROL_TEXT_1)
        morph_linear->set_control_type (MorphLinear::CONTROL_SIGNAL_1);
      else if (text == CONTROL_TEXT_2)
        morph_linear->set_control_type (MorphLinear::CONTROL_SIGNAL_2);
      else
        {
          assert (false);
        }
    }
  update_slider();
}

void
MorphLinearView::update_slider()
{
  if (morph_linear->control_type() == MorphLinear::CONTROL_GUI)
    {
      morphing_stack->setCurrentIndex (0);
    }
  else
    {
      morphing_stack->setCurrentIndex (1);
    }
}

void
MorphLinearView::on_operator_changed()
{
  morph_linear->set_left_op (left_combobox->active());
  morph_linear->set_right_op (right_combobox->active());
}

void
MorphLinearView::on_db_linear_changed (bool new_value)
{
  morph_linear->set_db_linear (new_value);
}

void
MorphLinearView::on_use_lpc_changed (bool new_value)
{
  morph_linear->set_use_lpc (new_value);
}
