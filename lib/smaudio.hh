// Licensed GNU LGPL v2.1 or later: http://www.gnu.org/licenses/lgpl-2.1.html

#ifndef SPECTMORPH_AUDIO_HH
#define SPECTMORPH_AUDIO_HH

#include <vector>

#include "smgenericin.hh"
#include "smgenericout.hh"
#include "smmath.hh"
#include "smutils.hh"

#define SPECTMORPH_BINARY_FILE_VERSION   14
#define SPECTMORPH_SUPPORT_MULTI_CHANNEL 0

namespace SpectMorph
{

/**
 * \brief Block of audio data, encoded in SpectMorph parametric format
 *
 * This represents a single analysis frame, usually containing sine waves in freqs
 * and phases, and a noise envelope for everything that remained after subtracting
 * the sine waves. The parameters original_fft and debug_samples are optional and
 * are used for debugging only.
 */
class AudioBlock
{
public:
  std::vector<uint16_t> noise;       //!< noise envelope, representing the original signal minus sine components
  std::vector<uint16_t> freqs;       //!< frequencies of the sine components of this frame
  std::vector<uint16_t> mags;        //!< magnitudes of the sine components
  std::vector<uint16_t> phases;      //!< phases of the sine components
  std::vector<float> original_fft;   //!< original zeropadded FFT data - for debugging only
  std::vector<float> debug_samples;  //!< original audio samples for this frame - for debugging only

  void    sort_freqs();
  double  estimate_fundamental (int n_partials = 1, double *mag = nullptr) const;

  double
  freqs_f (size_t i) const
  {
    return sm_ifreq2freq (freqs[i]);
  }

  double
  mags_f (size_t i) const
  {
    return sm_idb2factor (mags[i]);
  }

  double
  phases_f (size_t i) const
  {
    const double factor = 2.0 * M_PI / 65536.0;
    return phases[i] * factor;
  }

  double
  noise_f (size_t i) const
  {
    return sm_idb2factor (noise[i]);
  }
};

enum AudioLoadOptions
{
  AUDIO_LOAD_DEBUG,
  AUDIO_SKIP_DEBUG
};

/**
 * \brief Audio sample containing many blocks
 *
 * This class contains the information the SpectMorph::Encoder creates for a wav file. The
 * time dependant parameters are stored in contents, as a vector of audio frames; the
 * parameters that are the same for all frames are stored in this class.
 */
class Audio
{
  SPECTMORPH_CLASS_NON_COPYABLE (Audio);
public:
  Audio();
  ~Audio();

  enum LoopType {
    LOOP_NONE = 0,
    LOOP_FRAME_FORWARD,
    LOOP_FRAME_PING_PONG,
    LOOP_TIME_FORWARD,
    LOOP_TIME_PING_PONG,
  };

  float    fundamental_freq         = 0;          //!< fundamental frequency (note which was encoded), or 0 if not available
  float    mix_freq                 = 0;          //!< mix freq (sampling rate) of the original audio data
  float    frame_size_ms            = 0;          //!< length of each audio frame in milliseconds
  float    frame_step_ms            = 0;          //!< stepping of the audio frames in milliseconds
  float    attack_start_ms          = 0;          //!< start of attack in milliseconds
  float    attack_end_ms            = 0;          //!< end of attack in milliseconds
  int      zeropad                  = 0;          //!< FFT zeropadding used during analysis
  LoopType loop_type                = LOOP_NONE;  //!< type of loop to be used during sustain phase of playback
  int      loop_start               = 0;          //!< loop point to be used during sustain phase of playback
  int      loop_end                 = 0;          //!< loop point to be used during sustain phase of playback
  int      zero_values_at_start     = 0;          //!< number of zero values added by encoder (strip during decoding)
  int      sample_count             = 0;          //!< number of samples encoded (including zero_values_at_start)
  std::vector<float> original_samples;            //!< original time domain signal as samples (debugging only)
  float    original_samples_norm_db = 0;          //!< normalization factor to be applied to original samples
  std::vector<AudioBlock> contents;               //!< the actual frame data

  Error load (const std::string& filename, AudioLoadOptions load_options = AUDIO_LOAD_DEBUG);
  Error load (SpectMorph::GenericIn *file, AudioLoadOptions load_options = AUDIO_LOAD_DEBUG);
  Error save (const std::string& filename) const;
  Error save (SpectMorph::GenericOut *file) const;

  Audio *clone() const; // create a deep copy

  static bool loop_type_to_string (LoopType loop_type, std::string& s);
  static bool string_to_loop_type (const std::string& s, LoopType& loop_type);
};

}

#endif
