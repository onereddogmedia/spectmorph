// Licensed GNU LGPL v2.1 or later: http://www.gnu.org/licenses/lgpl-2.1.html

#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>

#include "smlv2common.hh"
#include "smlv2ui.hh"
#include "smlv2plugin.hh"
#include "smmemout.hh"
#include "smhexstring.hh"
#include "smutils.hh"
#include "smmain.hh"

using namespace SpectMorph;

using std::vector;
using std::string;

LV2UI::LV2UI (PuglNativeWindow parent_win_id, LV2UI_Resize *ui_resize, LV2Plugin *plugin) :
  plugin (plugin),
  ui_resize (ui_resize)
{
  window = new MorphPlanWindow (event_loop, "SpectMorph LV2", parent_win_id, /* resize */ false, plugin->project.morph_plan());

  connect (window->signal_update_size, this, &LV2UI::on_update_window_size);

  window->show();
}

void
LV2UI::on_update_window_size()
{
  if (ui_resize)
    {
      int width, height;
      window->get_scaled_size (&width, &height);

      ui_resize->ui_resize (ui_resize->handle, width, height);
    }
}

LV2UI::~LV2UI()
{
  delete window;
  window = nullptr;
}

void
LV2UI::idle()
{
  event_loop.process_events();
}

static LV2UI_Handle
instantiate(const LV2UI_Descriptor*   descriptor,
            const char*               plugin_uri,
            const char*               bundle_path,
            LV2UI_Write_Function      write_function,
            LV2UI_Controller          controller,
            LV2UI_Widget*             widget,
            const LV2_Feature* const* features)
{
  sm_plugin_init();

  LV2_DEBUG ("instantiate called for ui\n");

  LV2Plugin *plugin = nullptr;
  PuglNativeWindow parent_win_id = 0;
  LV2_URID_Map* map    = nullptr;
  LV2UI_Resize *ui_resize = nullptr;
  for (int i = 0; features[i]; i++)
    {
      if (!strcmp (features[i]->URI, LV2_URID__map))
        {
          map = (LV2_URID_Map*)features[i]->data;
        }
      else if (!strcmp (features[i]->URI, LV2_UI__parent))
        {
          parent_win_id = (PuglNativeWindow)features[i]->data;
          LV2_DEBUG ("Parent X11 ID %" PRIuPTR "\n", parent_win_id);
        }
      else if (!strcmp (features[i]->URI, LV2_UI__resize))
        {
          ui_resize = (LV2UI_Resize*)features[i]->data;
        }
      else if (!strcmp (features[i]->URI, LV2_INSTANCE_ACCESS_URI))
        {
          plugin = (LV2Plugin *) features[i]->data;
        }
    }
  if (!map)
    {
      return nullptr; // host bug, we need this feature
    }
  LV2UI *ui = new LV2UI (parent_win_id, ui_resize, plugin);
  ui->init_map (map);

  *widget = (void *)ui->window->native_window();

  /* set initial window size */
  ui->on_update_window_size();

  return ui;
}

static void
cleanup (LV2UI_Handle handle)
{
  LV2_DEBUG ("cleanup called for ui\n");

  LV2UI *ui = static_cast <LV2UI *> (handle);
  delete ui;

  sm_plugin_cleanup();
}

void
LV2UI::port_event (uint32_t     port_index,
                   uint32_t     buffer_size,
                   uint32_t     format,
                   const void*  buffer)
{
}

static void
port_event(LV2UI_Handle handle,
           uint32_t     port_index,
           uint32_t     buffer_size,
           uint32_t     format,
           const void*  buffer)
{
  LV2UI *ui = static_cast <LV2UI *> (handle);
  ui->port_event (port_index, buffer_size, format, buffer);

}

static int
idle (LV2UI_Handle handle)
{
  LV2UI* ui = (LV2UI*) handle;

  ui->idle();
  return 0;
}

static const LV2UI_Idle_Interface idle_iface = { idle };

static const void*
extension_data (const char* uri)
{
  // could implement show interface

  if (!strcmp(uri, LV2_UI__idleInterface)) {
    return &idle_iface;
  }
  return nullptr;
}

static const LV2UI_Descriptor descriptor = {
  SPECTMORPH_UI_URI,
  instantiate,
  cleanup,
  port_event,
  extension_data
};

LV2_SYMBOL_EXPORT
const LV2UI_Descriptor*
lv2ui_descriptor (uint32_t index)
{
  switch (index)
    {
      case 0:   return &descriptor;
      default:  return NULL;
    }
}
