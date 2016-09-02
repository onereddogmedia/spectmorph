// Licensed GNU LGPL v3 or later: http://www.gnu.org/licenses/lgpl.html

#include "smvstui.hh"
#include "smvstplugin.hh"
#include "smmemout.hh"

#include <QWindow>
#include <QPushButton>
#include <QApplication>

using namespace SpectMorph;

using std::string;
using std::vector;

VstUI::VstUI (const string& filename, VstPlugin *plugin) :
  morph_plan (new MorphPlan()),
  plugin (plugin)
{
  GenericIn *in = StdioIn::open (filename);
  if (!in)
    {
      g_printerr ("Error opening '%s'.\n", filename.c_str());
      exit (1);
    }
  morph_plan->load (in);
  delete in;
}

bool
VstUI::open (WId win_id)
{
  widget = new MorphPlanWindow (morph_plan, "!title!");
  widget->winId();
  widget->windowHandle()->setParent (QWindow::fromWinId (win_id));
  widget->show();
  rectangle.top = 0;
  rectangle.left = 0;
  rectangle.bottom = widget->height();
  rectangle.right = widget->width();

  connect (morph_plan.c_ptr(), SIGNAL (plan_changed()), this, SLOT (on_plan_changed()));

  return true;
}

bool
VstUI::getRect (ERect** rect)
{
  *rect = &rectangle;

  return true;
}

void
VstUI::close()
{
  delete widget;
}

void
VstUI::idle()
{
  QApplication::processEvents();
}

void
VstUI::on_plan_changed()
{
  // provide MorphPlan::clone() const function
  MorphPlanPtr plan_clone = new MorphPlan();

  vector<unsigned char> data;
  MemOut mo (&data);
  morph_plan->save (&mo);

  GenericIn *in = MMapIn::open_mem (&data[0], &data[data.size()]);
  plan_clone->load (in);
  delete in;

  plugin->change_plan (plan_clone);
}
