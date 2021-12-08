// Licensed GNU LGPL v2.1 or later: http://www.gnu.org/licenses/lgpl-2.1.html

#include "smmain.hh"
#include "smwavsetbuilder.hh"
#include "sminstenccache.hh"

#include <assert.h>

using namespace SpectMorph;
using std::vector;

int
main (int argc, char **argv)
{
  Main main (&argc, &argv);

  if (argc == 3 && strcmp (argv[1], "kill") == 0)
    {
      Instrument inst;
      inst.load (argv[2]);

      WavSetBuilder builder (&inst, /* keep_samples */ false);

      auto kill_func = []() {
        static double last_t = -1;
        double t = get_time();
        if (last_t > 0)
          sm_printf ("%f\n", (t - last_t) * 1000); // ms
        last_t = t;
        return false;
      };
      builder.set_kill_function (kill_func);

      kill_func(); // take time at start
      std::unique_ptr<WavSet> wav_set (builder.run());
      kill_func(); // take time at end

      return 0;
    }
  assert (argc == 2);

  vector<double> times;

  for (int i = 0; i < 10; i++)
    {
      // pretend that the just program started, and cache doesn't have in-memory entries
      InstEncCache::the()->clear();

      double t = get_time();

      Instrument inst;
      inst.load (argv[1]);

      WavSetBuilder builder (&inst, /* keep_samples */ false);
      std::unique_ptr<WavSet> wav_set (builder.run());

      times.push_back ((get_time() - t) * 1000);
    }

  // report times at end of test
  for (auto t_ms : times)
      printf ("time: %.2f ms\n", t_ms);
}
