// Licensed GNU LGPL v2.1 or later: http://www.gnu.org/licenses/lgpl-2.1.html

#include "sminsteditsynth.hh"
#include "smleakdebugger.hh"

using namespace SpectMorph;

using std::string;
using std::vector;

static LeakDebugger leak_debugger ("SpectMorph::InstEditSynth");

InstEditSynth::InstEditSynth (float mix_freq) :
  mix_freq (mix_freq)
{
  leak_debugger.add (this);

  unsigned int voices_per_layer = 64;
  for (unsigned int v = 0; v < voices_per_layer; v++)
    {
      for (unsigned int layer = 0; layer < n_layers; layer++)
        {
          Voice voice;
          voice.layer = layer;

          voices.push_back (std::move (voice));
        }
    }
}

InstEditSynth::~InstEditSynth()
{
  leak_debugger.del (this);
}

void
InstEditSynth::take_wav_sets (WavSet *new_wav_set, WavSet *new_ref_wav_set)
{
  wav_set.reset (new_wav_set);
  ref_wav_set.reset (new_ref_wav_set);
  for (auto& voice : voices)
    {
      if (voice.layer == 0)
        {
          voice.decoder.reset (new LiveDecoder (wav_set.get()));
        }
      if (voice.layer == 1)
        {
          voice.decoder.reset (new LiveDecoder (wav_set.get()));
          voice.decoder->enable_original_samples (true);
        }
      if (voice.layer == 2)
        {
          voice.decoder.reset (new LiveDecoder (ref_wav_set.get()));
        }
    }
}

static double
note_to_freq (int note)
{
  return 440 * exp (log (2) * (note - 69) / 12.0);
}

void
InstEditSynth::handle_midi_event (const unsigned char *midi_data, unsigned int layer)
{
  const unsigned char status = (midi_data[0] & 0xf0);
  /* note on: */
  if (status == 0x90 && midi_data[2] != 0) /* note on with velocity 0 => note off */
    {
      for (auto& voice : voices)
        {
          if (voice.decoder && voice.state == State::IDLE && voice.layer == layer)
            {
              voice.decoder->retrigger (0, note_to_freq (midi_data[1]), 127, mix_freq);
              voice.decoder_factor = 1;
              voice.state = State::ON;
              voice.note = midi_data[1];
              return; /* found free voice */
            }
        }
    }

  /* note off */
  if (status == 0x80 || (status == 0x90 && midi_data[2] == 0))
    {
      for (auto& voice : voices)
        {
          if (voice.state == State::ON && voice.note == midi_data[1] && voice.layer == layer)
            voice.state = State::RELEASE;
        }
    }
}

void
InstEditSynth::process (float *output, size_t n_values)
{
  int   iev_note[voices.size()];
  int   iev_layer[voices.size()];
  float iev_pos[voices.size()];
  float iev_fnote[voices.size()];
  int   iev_len = 0;

  zero_float_block (n_values, output);
  for (auto& voice : voices)
    {
      if (voice.decoder && voice.state != State::IDLE)
        {
          iev_note[iev_len]  = voice.note;
          iev_layer[iev_len] = voice.layer;
          iev_pos[iev_len]   = voice.decoder->current_pos();
          iev_fnote[iev_len] = voice.decoder->fundamental_note();
          iev_len++;

          float samples[n_values];

          voice.decoder->process (n_values, nullptr, &samples[0]);

          const float release_ms = 150;
          const float decrement = (1000.0 / mix_freq) / release_ms;

          for (size_t i = 0; i < n_values; i++)
            {
              if (voice.state == State::ON)
                {
                  output[i] += samples[i]; /* pass */
                }
              else if (voice.state == State::RELEASE)
                {
                  voice.decoder_factor -= decrement;

                  if (voice.decoder_factor > 0)
                    output[i] += samples[i] * voice.decoder_factor;
                  else
                    voice.state = State::IDLE;
                }
            }
          if (voice.decoder->done())
            voice.state = State::IDLE;
        }
    }

  notify_buffer.write_start ("InstEditVoice");
  notify_buffer.write_int_seq (iev_note, iev_len);
  notify_buffer.write_int_seq (iev_layer, iev_len);
  notify_buffer.write_float_seq (iev_pos, iev_len);
  notify_buffer.write_float_seq (iev_fnote, iev_len);
  notify_buffer.write_end();

  out_events.push_back (notify_buffer.to_string());
}

vector<string>
InstEditSynth::take_out_events()
{
  return std::move (out_events);
}
