// Licensed GNU LGPL v2.1 or later: http://www.gnu.org/licenses/lgpl-2.1.html

#ifndef SPECTMORPH_MORPH_LFO_VIEW_HH
#define SPECTMORPH_MORPH_LFO_VIEW_HH

#include "smmorphoperatorview.hh"
#include "smmorphlfo.hh"
#include "smcomboboxoperator.hh"
#include "smpropertyview.hh"
#include "smoperatorlayout.hh"

namespace SpectMorph
{

class MorphLFOView : public MorphOperatorView
{
  MorphLFO  *morph_lfo;

  Label     *note_label;
  Widget    *note_widget;

  PropertyView *pv_frequency;

  OperatorLayout op_layout;

  void update_visible();
public:
  MorphLFOView (Widget *widget, MorphLFO *op, MorphPlanWindow *morph_plan_window);

  double view_height() override;
};

}

#endif
