// Licensed GNU LGPL v2.1 or later: http://www.gnu.org/licenses/lgpl-2.1.html

#include "smleakdebugger.hh"
#include "smmain.hh"
#include "smdebug.hh"
#include <assert.h>
#include <glib.h>

#define DEBUG (1)

using namespace SpectMorph;

using std::string;
using std::map;

void
LeakDebugger::ptr_add (void *p)
{
  if (DEBUG)
    {
      assert (sm_init_done());

      std::lock_guard<std::mutex> lock (mutex);

      if (ptr_map[p] != 0)
        g_critical ("LeakDebugger: invalid registration of object type %s detected; ptr_map[p] is %d\n",
                    type.c_str(), ptr_map[p]);

      ptr_map[p]++;
    }
}

void
LeakDebugger::ptr_del (void *p)
{
  if (DEBUG)
    {
      assert (sm_init_done());

      std::lock_guard<std::mutex> lock (mutex);

      if (ptr_map[p] != 1)
        g_critical ("LeakDebugger: invalid deletion of object type %s detected; ptr_map[p] is %d\n",
                    type.c_str(), ptr_map[p]);

      ptr_map[p]--;
    }
}

LeakDebugger::LeakDebugger (const string& name, std::function<void()> cleanup_function) :
  type (name),
  cleanup_function (cleanup_function)
{
}

LeakDebugger::~LeakDebugger()
{
  if (DEBUG)
    {
      if (cleanup_function)
        cleanup_function();

      int alive = 0;

      for (map<void *, int>::iterator pi = ptr_map.begin(); pi != ptr_map.end(); pi++)
        {
          if (pi->second != 0)
            {
              assert (pi->second == 1);
              alive++;
            }
        }
      if (alive)
        {
          g_printerr ("LeakDebugger (%s) => %d objects remaining\n", type.c_str(), alive);
          sm_debug ("LeakDebugger (%s) => %d objects remaining\n", type.c_str(), alive);
        }
    }
}
