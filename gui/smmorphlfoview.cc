/*
 * Copyright (C) 2011 Stefan Westerfeld
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "smmorphlfoview.hh"
#include "smmorphplan.hh"
#include <birnet/birnet.hh>

using namespace SpectMorph;

using std::string;
using std::vector;

#define WAVE_TEXT_SINE     "Sine"
#define WAVE_TEXT_TRIANGLE "Triangle"

MorphLFOView::MorphLFOView (MorphLFO *morph_lfo, MorphPlanWindow *morph_plan_window) :
  MorphOperatorView (morph_lfo, morph_plan_window),
  morph_lfo (morph_lfo),
  frequency_scale (-2, 1, 0.0001)
{
  wave_type_combobox.append_text (WAVE_TEXT_SINE);
  wave_type_combobox.append_text (WAVE_TEXT_TRIANGLE);
  wave_type_label.set_text ("Wave Type");

  if (morph_lfo->wave_type() == MorphLFO::WAVE_SINE)
    wave_type_combobox.set_active_text (WAVE_TEXT_SINE);
  else if (morph_lfo->wave_type() == MorphLFO::WAVE_TRIANGLE)
    wave_type_combobox.set_active_text (WAVE_TEXT_TRIANGLE);
  else
    {
      assert (false);
    }

  table.attach (wave_type_label, 0, 1, 0, 1, Gtk::SHRINK);
  table.attach (wave_type_combobox, 1, 2, 0, 1);

  frequency_scale.signal_value_changed().connect (sigc::mem_fun (*this, &MorphLFOView::on_frequency_changed));
  frequency_label.set_text ("Frequency");
  frequency_scale.set_draw_value (false);
  frequency_hbox.pack_start (frequency_scale);
  frequency_hbox.pack_start (frequency_value_label, Gtk::PACK_SHRINK);
  frequency_hbox.set_spacing (10);
  frequency_scale.set_value (log10 (morph_lfo->frequency()));

  table.attach (frequency_label, 0, 1, 1, 2, Gtk::SHRINK);
  table.attach (frequency_hbox, 1, 2, 1, 2);

  table.set_spacings (10);
  table.set_border_width (5);

  frame.add (table);

  wave_type_combobox.signal_changed().connect (sigc::mem_fun (*this, &MorphLFOView::on_wave_type_changed));

  show_all_children();
}

MorphLFOView::~MorphLFOView()
{
}

void
MorphLFOView::on_wave_type_changed()
{
  string text = wave_type_combobox.get_active_text();

  if (text == WAVE_TEXT_SINE)
    morph_lfo->set_wave_type (MorphLFO::WAVE_SINE);
  else if (text == WAVE_TEXT_TRIANGLE)
    morph_lfo->set_wave_type (MorphLFO::WAVE_TRIANGLE);
  else
    {
      assert (false);
    }
}

void
MorphLFOView::on_frequency_changed()
{
  double frequency = pow (10, frequency_scale.get_value());
  frequency_value_label.set_text (Birnet::string_printf ("%.3f Hz", frequency));
  morph_lfo->set_frequency (frequency);
}
