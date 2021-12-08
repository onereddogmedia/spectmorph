// Licensed GNU LGPL v2.1 or later: http://www.gnu.org/licenses/lgpl-2.1.html

#include "smmorphgridmodule.hh"
#include "smmorphgrid.hh"
#include "smmorphplanvoice.hh"
#include "smleakdebugger.hh"
#include "smmath.hh"
#include "smlivedecoder.hh"
#include "smmorphutils.hh"

#include <assert.h>

using namespace SpectMorph;

using std::min;
using std::max;
using std::vector;
using std::string;
using std::sort;

static LeakDebugger leak_debugger ("SpectMorph::MorphGridModule");

MorphGridModule::MorphGridModule (MorphPlanVoice *voice) :
  MorphOperatorModule (voice)
{
  leak_debugger.add (this);

  my_source.module = this;

  audio.fundamental_freq     = 440;
  audio.mix_freq             = 48000;
  audio.frame_size_ms        = 1;
  audio.frame_step_ms        = 1;
  audio.zeropad              = 4;
  audio.loop_type            = Audio::LOOP_NONE;
}

MorphGridModule::~MorphGridModule()
{
  leak_debugger.del (this);
}

void
MorphGridModule::set_config (const MorphOperatorConfig *op_cfg)
{
  cfg = dynamic_cast<const MorphGrid::Config *> (op_cfg);
  g_return_if_fail (cfg != NULL);

  input_node.resize (cfg->width);

  for (int x = 0; x < cfg->width; x++)
    {
      input_node[x].resize (cfg->height);
      for (int y = 0; y < cfg->height; y++)
        {
          const MorphGridNode& node = cfg->input_node[x][y];

          input_node[x][y].mod = morph_plan_voice->module (node.op);

          if (node.path != "")
            {
              input_node[x][y].source.set_wav_set (node.path);
              input_node[x][y].has_source = true;
            }
          else
            {
              input_node[x][y].has_source = false;
            }

          input_node[x][y].delta_db = node.delta_db;
        }
    }
}

void
MorphGridModule::MySource::retrigger (int channel, float freq, int midi_velocity, float mix_freq)
{
  for (int x = 0; x < module->cfg->width; x++)
    {
      for (int y = 0; y < module->cfg->height; y++)
        {
          InputNode& node = module->input_node[x][y];

          if (node.mod && node.mod->source())
            {
              node.mod->source()->retrigger (channel, freq, midi_velocity, mix_freq);
            }
          if (node.has_source)
            {
              node.source.retrigger (channel, freq, midi_velocity, mix_freq);
            }
        }
    }
}

Audio*
MorphGridModule::MySource::audio()
{
  return &module->audio;
}

static bool
get_normalized_block (MorphGridModule::InputNode& input_node, size_t index, AudioBlock& out_audio_block)
{
  LiveDecoderSource *source = NULL;

  if (input_node.mod)
    {
      source = input_node.mod->source();
    }
  else if (input_node.has_source)
    {
      source = &input_node.source;
    }
  const double time_ms = index; // 1ms frame step

  return MorphUtils::get_normalized_block (source, time_ms, out_audio_block);
}

namespace
{
struct MagData
{
  enum {
    BLOCK_LEFT  = 0,
    BLOCK_RIGHT = 1
  }       block;
  size_t   index;
  uint16_t mag;
};

static bool
md_cmp (const MagData& m1, const MagData& m2)
{
  return m1.mag > m2.mag;  // sort with biggest magnitude first
}

static void
interp_mag_one (double interp, uint16_t *left, uint16_t *right)
{
  const uint16_t lmag_idb = max<uint16_t> (left ? *left : 0, SM_IDB_CONST_M96);
  const uint16_t rmag_idb = max<uint16_t> (right ? *right : 0, SM_IDB_CONST_M96);

  const uint16_t mag_idb = sm_round_positive ((1 - interp) * lmag_idb + interp * rmag_idb);

  if (left)
    *left = mag_idb;
  if (right)
    *right = mag_idb;
}

static void
morph_scale (AudioBlock& out_block, const AudioBlock& in_block, double factor)
{
  const int ddb = sm_factor2delta_idb (factor);

  out_block = in_block;
  for (size_t i = 0; i < out_block.noise.size(); i++)
    out_block.noise[i] = sm_bound<int> (0, out_block.noise[i] + ddb, 65535);

  for (size_t i = 0; i < out_block.freqs.size(); i++)
    interp_mag_one (factor, NULL, &out_block.mags[i]);
}

}

static bool
morph (AudioBlock& out_block,
       bool have_left, const AudioBlock& left_block,
       bool have_right, const AudioBlock& right_block,
       double morphing)
{
  const double interp = (morphing + 1) / 2; /* examples => 0: only left; 0.5 both equally; 1: only right */

  if (!have_left && !have_right) // nothing + nothing = nothing
    return false;

  if (!have_left) // nothing + interp * right = interp * right
    {
      morph_scale (out_block, right_block, interp);
      return true;
    }
  if (!have_right) // (1 - interp) * left + nothing = (1 - interp) * left
    {
      morph_scale (out_block, left_block, 1 - interp);
      return true;
    }

  // clear result block
  out_block.freqs.clear();
  out_block.mags.clear();

  // FIXME: lpc stuff
  MagData mds[left_block.freqs.size() + right_block.freqs.size()];
  size_t  mds_size = 0;
  for (size_t i = 0; i < left_block.freqs.size(); i++)
    {
      MagData& md = mds[mds_size];

      md.block = MagData::BLOCK_LEFT;
      md.index = i;
      md.mag   = left_block.mags[i];
      mds_size++;
    }
  for (size_t i = 0; i < right_block.freqs.size(); i++)
    {
      MagData& md = mds[mds_size];

      md.block = MagData::BLOCK_RIGHT;
      md.index = i;
      md.mag   = right_block.mags[i];
      mds_size++;
    }
  sort (mds, mds + mds_size, md_cmp);

  size_t    left_freqs_size = left_block.freqs.size();
  size_t    right_freqs_size = right_block.freqs.size();

  MorphUtils::FreqState   left_freqs[left_freqs_size];
  MorphUtils::FreqState   right_freqs[right_freqs_size];

  init_freq_state (left_block.freqs, left_freqs);
  init_freq_state (right_block.freqs, right_freqs);

  for (size_t m = 0; m < mds_size; m++)
    {
      size_t i, j;
      bool match = false;
      if (mds[m].block == MagData::BLOCK_LEFT)
        {
          i = mds[m].index;

          if (!left_freqs[i].used)
            match = MorphUtils::find_match (left_freqs[i].freq_f, right_freqs, right_freqs_size, &j);
        }
      else // (mds[m].block == MagData::BLOCK_RIGHT)
        {
          j = mds[m].index;
          if (!right_freqs[j].used)
            match = MorphUtils::find_match (right_freqs[j].freq_f, left_freqs, left_freqs_size, &i);
        }
      if (match)
        {
          /* prefer frequency of louder partial:
           *
           * if the magnitudes are similar, mfact will be close to 1, and freq will become approx.
           *
           *   freq = (1 - interp) * lfreq + interp * rfreq
           *
           * if the magnitudes are very different, mfact will be close to 0, and freq will become
           *
           *   freq ~= lfreq         // if left partial is louder
           *   freq ~= rfreq         // if right partial is louder
           */
          const double lfreq = left_block.freqs[i];
          const double rfreq = right_block.freqs[j];
          double freq;

          if (left_block.mags[i] > right_block.mags[j])
            {
              const double mfact = right_block.mags_f (j) / left_block.mags_f (i);

              freq = lfreq + mfact * interp * (rfreq - lfreq);
            }
          else
            {
              const double mfact = left_block.mags_f (i) / right_block.mags_f (j);

              freq = rfreq + mfact * (1 - interp) * (lfreq - rfreq);
            }
          // FIXME: lpc
          // FIXME: non-db

          const uint16_t lmag_idb = max (left_block.mags[i], SM_IDB_CONST_M96);
          const uint16_t rmag_idb = max (right_block.mags[j], SM_IDB_CONST_M96);
          const uint16_t mag_idb = sm_round_positive ((1 - interp) * lmag_idb + interp * rmag_idb);

          out_block.freqs.push_back (freq);
          out_block.mags.push_back (mag_idb);

          left_freqs[i].used = 1;
          right_freqs[j].used = 1;
        }
    }
  for (size_t i = 0; i < left_freqs_size; i++)
    {
      if (!left_freqs[i].used)
        {
          out_block.freqs.push_back (left_block.freqs[i]);
          out_block.mags.push_back (left_block.mags[i]);

          interp_mag_one (interp, &out_block.mags.back(), NULL);
        }
    }
  for (size_t i = 0; i < right_freqs_size; i++)
    {
      if (!right_freqs[i].used)
        {
          out_block.freqs.push_back (right_block.freqs[i]);
          out_block.mags.push_back (right_block.mags[i]);

          interp_mag_one (interp, NULL, &out_block.mags.back());
        }
    }
  out_block.noise.clear();
  for (size_t i = 0; i < left_block.noise.size(); i++)
    out_block.noise.push_back (sm_factor2idb ((1 - interp) * left_block.noise_f (i) + interp * right_block.noise_f (i)));

  out_block.sort_freqs();
  return true;
}


namespace
{

struct LocalMorphParams
{
  int     start;
  int     end;
  double  morphing;
};

static LocalMorphParams
global_to_local_params (double global_morphing, int node_count)
{
  LocalMorphParams result;

  /* interp: range for node_count=3: 0 ... 2.0 */
  const double interp = (global_morphing + 1) / 2 * (node_count - 1);

  // find the two adjecant nodes (double -> integer position)
  result.start = sm_bound<int> (0, interp, (node_count - 1));
  result.end   = sm_bound<int> (0, result.start + 1, (node_count - 1));

  const double interp_frac = sm_bound (0.0, interp - result.start, 1.0); /* position between adjecant nodes */
  result.morphing = interp_frac * 2 - 1; /* normalize fractional part to range -1.0 ... 1.0 */
  return result;
}

static double
morph_delta_db (double left_db, double right_db, double morphing)
{
  const double interp = (morphing + 1) / 2; /* examples => 0: only left; 0.5 both equally; 1: only right */
  return left_db * (1 - interp) + right_db * interp;
}

static void
apply_delta_db (AudioBlock& block, double delta_db)
{
  const double factor = db_to_factor (delta_db);
  const int    ddb    = sm_factor2delta_idb (factor);

  // apply delta db volume to partials & noise
  for (size_t i = 0; i < block.mags.size(); i++)
    block.mags[i] = sm_bound<int> (0, block.mags[i] + ddb, 65535);

  for (size_t i = 0; i < block.noise.size(); i++)
    block.noise[i] = sm_bound<int> (0, block.noise[i] + ddb, 65535);
}

}

AudioBlock *
MorphGridModule::MySource::audio_block (size_t index)
{
  const double x_morphing = module->apply_modulation (module->cfg->x_morphing_mod);
  const double y_morphing = module->apply_modulation (module->cfg->y_morphing_mod);

  const LocalMorphParams x_morph_params = global_to_local_params (x_morphing, module->cfg->width);
  const LocalMorphParams y_morph_params = global_to_local_params (y_morphing, module->cfg->height);

  if (module->cfg->height == 1)
    {
      /*
       *  A ---- B
       */

      InputNode& node_a = module->input_node[x_morph_params.start][0];
      InputNode& node_b = module->input_node[x_morph_params.end  ][0];

      bool have_a = get_normalized_block (node_a, index, audio_block_a);
      bool have_b = get_normalized_block (node_b, index, audio_block_b);

      bool have_ab = morph (module->audio_block, have_a, audio_block_a, have_b, audio_block_b, x_morph_params.morphing);

      double delta_db = morph_delta_db (node_a.delta_db, node_b.delta_db, x_morph_params.morphing);

      if (have_ab)
        apply_delta_db (module->audio_block, delta_db);

      return have_ab ? &module->audio_block : NULL;
    }
  else if (module->cfg->width == 1)
    {
      /*
       *  A
       *  |
       *  |
       *  B
       */

      InputNode& node_a = module->input_node[0][y_morph_params.start];
      InputNode& node_b = module->input_node[0][y_morph_params.end  ];

      bool have_a = get_normalized_block (node_a, index, audio_block_a);
      bool have_b = get_normalized_block (node_b, index, audio_block_b);

      bool have_ab = morph (module->audio_block, have_a, audio_block_a, have_b, audio_block_b, y_morph_params.morphing);

      double delta_db = morph_delta_db (node_a.delta_db, node_b.delta_db, y_morph_params.morphing);

      if (have_ab)
        apply_delta_db (module->audio_block, delta_db);

      return have_ab ? &module->audio_block : NULL;
    }
  else
    {
      /*
       *  A ---- B
       *  |      |
       *  |      |
       *  C ---- D
       */
      InputNode& node_a = module->input_node[x_morph_params.start][y_morph_params.start];
      InputNode& node_b = module->input_node[x_morph_params.end  ][y_morph_params.start];
      InputNode& node_c = module->input_node[x_morph_params.start][y_morph_params.end  ];
      InputNode& node_d = module->input_node[x_morph_params.end  ][y_morph_params.end  ];

      bool have_a = get_normalized_block (node_a, index, audio_block_a);
      bool have_b = get_normalized_block (node_b, index, audio_block_b);
      bool have_c = get_normalized_block (node_c, index, audio_block_c);
      bool have_d = get_normalized_block (node_d, index, audio_block_d);

      bool have_ab = morph (audio_block_ab, have_a, audio_block_a, have_b, audio_block_b, x_morph_params.morphing);
      bool have_cd = morph (audio_block_cd, have_c, audio_block_c, have_d, audio_block_d, x_morph_params.morphing);
      bool have_abcd = morph (module->audio_block, have_ab, audio_block_ab, have_cd, audio_block_cd, y_morph_params.morphing);

      double delta_db_ab = morph_delta_db (node_a.delta_db, node_b.delta_db, x_morph_params.morphing);
      double delta_db_cd = morph_delta_db (node_c.delta_db, node_d.delta_db, x_morph_params.morphing);
      double delta_db_abcd = morph_delta_db (delta_db_ab, delta_db_cd, y_morph_params.morphing);

      if (have_abcd)
        apply_delta_db (module->audio_block, delta_db_abcd);

      return have_abcd ? &module->audio_block : NULL;
    }
}

LiveDecoderSource *
MorphGridModule::source()
{
  return &my_source;
}
