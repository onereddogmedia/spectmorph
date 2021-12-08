// Licensed GNU LGPL v2.1 or later: http://www.gnu.org/licenses/lgpl-2.1.html

#ifndef SPECTMORPH_PROPERTY_VIEW_EDIT_HH
#define SPECTMORPH_PROPERTY_VIEW_EDIT_HH

#include "smwindow.hh"
#include "smbutton.hh"
#include "smlineedit.hh"
#include "smcontrolview.hh"
#include "smparamlabel.hh"
#include "smtoolbutton.hh"

namespace SpectMorph
{

class PropertyViewEdit : public Window
{
protected:
  Window         *parent_window;
  Property&       property;
  ModulationList *mod_list = nullptr;

  Button         *ok_button;
  Button         *add_mod_button;
  LineEdit       *line_edit;
  bool            line_edit_changed = false;
  ControlView     main_control_view;

  std::vector<ControlView *> control_views;
  std::vector<Widget *> mod_widgets;

  PropertyViewEdit (Window *parent, Property& property) :
    Window (*parent->event_loop(), "Edit Property", 560, 320, 0, false, parent->native_window()),
    parent_window (parent),
    property (property)
  {
    FixedGrid grid;

    double yoffset = 2;

    mod_list = property.modulation_list();
    if (mod_list)
      {
        auto control_combobox = main_control_view.create_combobox (this,
          property.op(),
          mod_list->main_control_type(),
          mod_list->main_control_op());

        connect (main_control_view.signal_control_changed,
          [this]()
            {
              mod_list->set_main_control_type_and_op (main_control_view.control_type(), main_control_view.op());
            });

        connect (mod_list->signal_main_control_changed, [this]()
          {
            main_control_view.update_control_type_and_op (mod_list->main_control_type(), mod_list->main_control_op());
          });

        grid.add_widget (control_combobox, 3, yoffset, 17, 3);
        yoffset += 3;
      }

    grid.add_widget (new Label (this, property.label()), 1, yoffset, 30, 3);
    if (property.type() == Property::Type::FLOAT)
      {
        grid.add_widget (new Label (this, property.label()), 1, yoffset, 30, 3);
        line_edit = new LineEdit (this, string_locale_printf ("%.3f", property.get_float()));
        line_edit->select_all();
        grid.add_widget (line_edit, 10, yoffset, 20, 3);
        line_edit->set_click_to_focus (true);
        connect (line_edit->signal_text_changed, [this](std::string s) { line_edit_changed = true; });
        set_keyboard_focus (line_edit, true);
      }

    yoffset += 3;


    add_mod_button = new Button (this, "Add Modulation");
    connect (add_mod_button->signal_clicked, [this]() {
      ModulationList *mod_list = this->property.modulation_list();
      if (mod_list)
        mod_list->add_entry();
    });

    ok_button = new Button (this, "Close");

    if (mod_list)
      connect (mod_list->signal_size_changed, this, &PropertyViewEdit::update_modulation_widgets);

    update_modulation_widgets();

    connect (line_edit->signal_return_pressed,  [=]() {
      if (ok_button->enabled())
        on_accept();
    });
    connect (ok_button->signal_clicked,     this, &PropertyViewEdit::on_accept);

    set_close_callback ([this]() { on_accept(); });

    show();
  }

  void
  update_modulation_widgets()
  {
    FixedGrid grid;

    auto mod_list = property.modulation_list();
    if (!mod_list)
      return;

    int yoffset = 10;
    // remove old modulation widgets created before
    for (auto w : mod_widgets)
      w->delete_later();
    mod_widgets.clear();
    for (auto cv : control_views)
      delete cv;
    control_views.clear();

    for (size_t i = 0; i < mod_list->count(); i++)
      {
        ModulationData::Entry e = (*mod_list)[i];

        // ===== remove entry button
        ToolButton *tbutton = new ToolButton (this, 'x');
        grid.add_widget (tbutton, 0.5, yoffset + 0.5, 2, 2);
        connect (tbutton->signal_clicked, [mod_list, i, this] () { mod_list->remove_entry (i); });

        // ===== modulation entry control op / type
        ControlView *control_view = new ControlView();
        auto control_combobox = control_view->create_combobox (this,
          property.op(),
          e.control_type,
          e.control_op.get());
        control_views.push_back (control_view);

        connect (control_view->signal_control_changed,
          [control_view, mod_list, i]()
            {
              ModulationData::Entry entry = (*mod_list)[i];

              entry.control_type = control_view->control_type();
              entry.control_op.set (control_view->op());

              mod_list->update_entry (i, entry);
            });

        grid.add_widget (control_combobox, 3, yoffset, 17, 3);

        // ===== unipolar / bipolar combobox

        static constexpr auto CB_UNIPOLAR_TEXT = "unipolar";
        static constexpr auto CB_BIPOLAR_TEXT = "bipolar";

        ComboBox *polarity_combobox = new ComboBox (this);
        polarity_combobox->add_item (CB_UNIPOLAR_TEXT);
        polarity_combobox->add_item (CB_BIPOLAR_TEXT);
        polarity_combobox->set_text (e.bipolar ? CB_BIPOLAR_TEXT : CB_UNIPOLAR_TEXT);

        grid.add_widget (polarity_combobox, 21, yoffset, 14, 3);

        connect (polarity_combobox->signal_item_changed,
          [polarity_combobox, mod_list, i]()
            {
              ModulationData::Entry entry = (*mod_list)[i];
              entry.bipolar = (polarity_combobox->text() == CB_BIPOLAR_TEXT);
              mod_list->update_entry (i, entry);
            });

        // ===== mod amount slider and value label
        auto slider = new Slider (this, (e.amount + 1) / 2);
        grid.add_widget (slider, 36, yoffset, 22, 3);

        auto mod_amount_model = new ParamLabelModelDouble (e.amount, -1, 1, "%.3f", "%.3f");
        auto label = new ParamLabel (this, mod_amount_model);
        grid.add_widget (label, 59, yoffset, 8, 3);

        connect (slider->signal_value_changed, [label, slider, mod_amount_model, mod_list, i](double new_value) {
          ModulationData::Entry entry = (*mod_list)[i];
          entry.amount = new_value * 2 - 1;
          mod_amount_model->set_value (entry.amount);
          mod_list->update_entry (i, entry);
        });

        connect (mod_amount_model->signal_value_changed, [mod_list, i, slider](double new_value) {
          ModulationData::Entry entry = (*mod_list)[i];
          entry.amount = new_value;
          slider->set_value ((entry.amount + 1) / 2);
          mod_list->update_entry (i, entry);
        });

        mod_widgets.push_back (tbutton);
        mod_widgets.push_back (control_combobox);
        mod_widgets.push_back (polarity_combobox);
        mod_widgets.push_back (slider);
        mod_widgets.push_back (label);
        yoffset += 3;
      }
    grid.add_widget (add_mod_button, 7, yoffset, 31, 3);
    yoffset += 3;
    grid.add_widget (ok_button, 17, yoffset, 10, 3);
  }
  void
  on_accept()
  {
    if (line_edit_changed)
      property.set_float (sm_atof_any (line_edit->text().c_str()));

    parent_window->set_popup_window (nullptr); // close this window
  }

  void
  on_reject()
  {
    parent_window->set_popup_window (nullptr); // close this window
  }

public:
  static void
  create (Window *window, Property& property)
  {
    Window *rwin = new PropertyViewEdit (window, property);

    // after this line, rename window is owned by parent window
    window->set_popup_window (rwin);
  }
};

}

#endif
