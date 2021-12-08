// Licensed GNU LGPL v2.1 or later: http://www.gnu.org/licenses/lgpl-2.1.html

#include "smadsrenvelope.hh"
#include "smutils.hh"
#include "smmath.hh"
#include "smmain.hh"

#include <vector>

#include <stdio.h>

using namespace SpectMorph;
using std::vector;

void
run (ADSREnvelope& adsr_envelope, size_t samples)
{
  vector<float> input (samples, 1.0);
  adsr_envelope.process (input.size(), &input[0]);
  for (auto v : input)
    printf ("%f\n", v);
}

int
main (int argc, char **argv)
{
  Main main (&argc, &argv);

  float rate = 48000;

  ADSREnvelope adsr_envelope;

  adsr_envelope.set_config (sm_atof (argv[1]), sm_atof (argv[2]), sm_atof (argv[3]), sm_atof (argv[4]), rate);
  adsr_envelope.retrigger();
  run (adsr_envelope, sm_round_positive (rate / 2));
  adsr_envelope.release();
  while (!adsr_envelope.done())
    {
      run (adsr_envelope, sm_round_positive (rate / 2));
    }
}
