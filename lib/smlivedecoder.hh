// Licensed GNU LGPL v3 or later: http://www.gnu.org/licenses/lgpl.html

#ifndef SPECTMORPH_LIVEDECODER_HH
#define SPECTMORPH_LIVEDECODER_HH

#include "smwavset.hh"
#include "smsinedecoder.hh"
#include "smnoisedecoder.hh"
#include "smlivedecodersource.hh"
#include "smpolyphaseinter.hh"
#include "smalignedarray.hh"
#include <vector>

namespace SpectMorph {

class LiveDecoder
{
  struct PartialState
  {
    float freq;
    float phase;
  };
  std::vector<PartialState> pstate[2], *last_pstate;

  struct PortamentoState {
    std::vector<float> buffer;
    double             pos;

    enum { DELTA = 32 };
  } portamento_state;

  WavSet             *smset;
  Audio              *audio;

  IFFTSynth          *ifft_synth;
  NoiseDecoder       *noise_decoder;
  LiveDecoderSource  *source;
  PolyPhaseInter     *pp_inter;

  bool                sines_enabled;
  bool                noise_enabled;
  bool                debug_fft_perf_enabled;
  bool                original_samples_enabled;
  bool                loop_enabled;

  size_t              frame_size, frame_step;
  size_t              zero_values_at_start_scaled;
  size_t              loop_start_scaled;
  size_t              loop_end_scaled;
  int                 loop_point;
  float               current_freq;
  float               current_mix_freq;

  size_t              have_samples;
  size_t              block_size;
  size_t              pos;
  double              env_pos;
  size_t              frame_idx;
  double              original_sample_pos;
  double              original_samples_norm_factor;

  int                 noise_seed;

  AlignedArray<float,16> *sse_samples;

  // unison
  int                 unison_voices;
  std::vector<float>  unison_phases[2];
  std::vector<float>  unison_freq_factor;
  float               unison_gain;
  Random              unison_phase_random_gen;

  // vibrato
  bool                vibrato_enabled;
  float               vibrato_depth;
  float               vibrato_frequency;
  float               vibrato_attack;

  Audio::LoopType     get_loop_type();

  void process_internal (size_t       n_values,
                         float       *audio_out,
                         float        portamento_stretch);

  void portamento_grow (double end_pos, float portamento_stretch);
  void portamento_shrink();

  void process_portamento (size_t       n_values,
                           const float *freq_in,
                           float       *audio_out);
public:
  LiveDecoder (WavSet *smset);
  LiveDecoder (LiveDecoderSource *source);
  ~LiveDecoder();

  void enable_noise (bool ne);
  void enable_sines (bool se);
  void enable_debug_fft_perf (bool dfp);
  void enable_original_samples (bool eos);
  void enable_loop (bool eloop);
  void set_noise_seed (int seed);
  void set_unison_voices (int voices, float detune);
  void set_vibrato (bool enable_vibrato, float depth, float frequency, float attack);

  void precompute_tables (float mix_freq);
  void retrigger (int channel, float freq, int midi_velocity, float mix_freq);
  void process (size_t       n_values,
                const float *freq_in,
                float       *audio_out);

  static size_t compute_loop_frame_index (size_t index, Audio *audio);
// later:  bool done();
};

}
#endif
