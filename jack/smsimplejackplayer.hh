// Licensed GNU LGPL v2.1 or later: http://www.gnu.org/licenses/lgpl-2.1.html

#ifndef SPECTMORPH_SIMPLE_JACK_PLAYER_HH
#define SPECTMORPH_SIMPLE_JACK_PLAYER_HH

#include <jack/jack.h>

#include <mutex>

#include "smlivedecoder.hh"

namespace SpectMorph
{

class SimpleJackPlayer
{
  jack_port_t        *audio_out_port;
  jack_client_t      *jack_client;

  std::mutex          decoder_mutex;
  LiveDecoder        *decoder;                  // decoder_mutex!
  Audio              *decoder_audio;            // decoder_mutex!
  LiveDecoderSource  *decoder_source;           // decoder_mutex!
  double              decoder_volume;           // decoder_mutex!
  bool                decoder_fade_out;         // decoder_mutex!
  double              decoder_fade_out_level;   // decoder_mutex!

  double              jack_mix_freq;

  void fade_out_blocking();
  void update_decoder (LiveDecoder *new_decoder, Audio *new_decoder_audio, LiveDecoderSource *new_decoder_source);
public:
  SimpleJackPlayer (const std::string& client_name);
  ~SimpleJackPlayer();

  void play (Audio *audio, bool use_samples);
  void stop();
  int  process (jack_nframes_t nframes);
  void set_volume (double new_volume);

  double mix_freq() const;
};

}

#endif
