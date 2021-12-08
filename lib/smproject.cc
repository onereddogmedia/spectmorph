// Licensed GNU LGPL v2.1 or later: http://www.gnu.org/licenses/lgpl-2.1.html

#include "smproject.hh"
#include "smmidisynth.hh"
#include "smsynthinterface.hh"
#include "smmorphoutputmodule.hh"
#include "smzip.hh"
#include "smmemout.hh"
#include "smmorphwavsource.hh"
#include "smuserinstrumentindex.hh"
#include "smproject.hh"

using namespace SpectMorph;

using std::string;
using std::vector;
using std::set;
using std::map;

void
ControlEventVector::take (SynthControlEvent *ev)
{
  // we'd rather run destructors in non-rt part of the code
  if (clear)
    {
      events.clear();
      clear = false;
    }

  events.emplace_back (ev);
}

void
ControlEventVector::run_rt (Project *project)
{
  if (!clear)
    {
      for (const auto& ev : events)
        ev->run_rt (project);

      clear = true;
    }
}

bool
Project::try_update_synth()
{
  bool state_changed = false;
  // handle synth updates (if locking is possible without blocking)
  //  - apply new parameters
  //  - process events
  if (m_synth_mutex.try_lock())
    {
      m_control_events.run_rt (this);
      m_out_events = m_midi_synth->inst_edit_synth()->take_out_events();
      m_voices_active = m_midi_synth->active_voice_count() > 0;
      state_changed = m_state_changed;
      m_state_changed = false;

      m_synth_mutex.unlock();
    }
  return state_changed;
}

void
Project::synth_take_control_event (SynthControlEvent *event)
{
  std::lock_guard<std::mutex> lg (m_synth_mutex);
  m_control_events.take (event);
}

void
Project::rebuild (MorphWavSource *wav_source)
{
  const int   object_id  = wav_source->object_id();
  Instrument *instrument = instrument_map[object_id].get();

  if (!instrument)
    return;

  WavSetBuilder *builder = new WavSetBuilder (instrument, /* keep_samples */ false);
  m_builder_thread.kill_jobs_by_id (object_id);
  synth_interface()->emit_add_rebuild_result (object_id, nullptr);
  m_builder_thread.add_job (builder, object_id,
    [this, object_id] (WavSet *wav_set)
      {
        synth_interface()->emit_add_rebuild_result (object_id, wav_set);
      });
}

bool
Project::rebuild_active (int object_id)
{
  if (object_id == 0)
    fprintf (stderr, "Project::rebuild_active (object_id = 0)\n");
  return m_builder_thread.search_job (object_id);
}

void
Project::add_rebuild_result (int object_id, WavSet *wav_set)
{
  size_t s = object_id + 1;
  if (s > wav_sets.size())
    wav_sets.resize (s);

  wav_sets[object_id] = std::shared_ptr<WavSet> (wav_set);
}

void
Project::clear_wav_sets()
{
  wav_sets.clear();
}

Instrument *
Project::get_instrument (MorphWavSource *wav_source)
{
  if (wav_source->object_id() == 0) /* create if not used */
    {
      /* check which object ids are currently used */
      set<int> used_object_ids;
      for (auto wav_source : list_wav_sources())
        {
          const int object_id = wav_source->object_id();

          if (object_id)
            {
              assert (instrument_map[object_id]); /* can only be used if it has a map entry */

              used_object_ids.insert (object_id);
            }
        }
      int object_id = 1;

      while (used_object_ids.count (object_id)) /* find first free slot */
        object_id++;

      wav_source->set_object_id (object_id);
      instrument_map[object_id].reset (new Instrument());
    }

  return instrument_map[wav_source->object_id()].get();
}

std::shared_ptr<WavSet>
Project::get_wav_set (int object_id)
{
  if (size_t (object_id) < wav_sets.size())
    return wav_sets[object_id];
  else
    return nullptr;
}

vector<string>
Project::notify_take_events()
{
  std::lock_guard<std::mutex> lg (m_synth_mutex);
  return std::move (m_out_events);
}

SynthInterface *
Project::synth_interface() const
{
  return m_synth_interface.get();
}

MidiSynth *
Project::midi_synth() const
{
  return m_midi_synth.get();
}

Project::Project()
{
  m_morph_plan = new MorphPlan (*this);
  m_morph_plan->load_default();

  connect (m_morph_plan->signal_plan_changed, this, &Project::on_plan_changed);
  connect (m_morph_plan->signal_operator_added, this, &Project::on_operator_added);
  connect (m_morph_plan->signal_operator_removed, this, &Project::on_operator_removed);

  m_synth_interface.reset (new SynthInterface (this));
}

void
Project::set_mix_freq (double mix_freq)
{
  // not rt safe, needs to be called when synthesis thread is not running
  m_midi_synth.reset (new MidiSynth (mix_freq, 64));
  m_mix_freq = mix_freq;

  // FIXME: can this cause problems if an old plan change control event remained
  auto update = m_midi_synth->prepare_update (m_morph_plan);
  m_midi_synth->apply_update (update);
  m_midi_synth->set_gain (db_to_factor (m_volume));
}

void
Project::set_storage_model (StorageModel model)
{
  m_storage_model = model;
}

void
Project::set_state_changed_notify (bool notify)
{
  m_state_changed_notify = notify;
}

void
Project::state_changed()
{
  if (m_state_changed_notify)
    m_state_changed = true;
}

void
Project::on_plan_changed()
{
  /* FIXME: CONFIG
   *
   * we used to save here anyway, so state changed check could be done easily
   * however now with fast config updates, we just save for state changed
   * checks which is a waste of time
   */
  // create a deep copy (by saving/loading)
  vector<unsigned char> plan_data;
  MemOut                plan_mo (&plan_data);

  m_morph_plan->save (&plan_mo);

  if (plan_data != m_last_plan_data)
    {
      m_last_plan_data = plan_data;
      state_changed();
    }

  // this might take a while, and cannot be done in synthesis thread
  MorphPlanSynth mp_synth (m_mix_freq, 1);
  {
    auto update = mp_synth.prepare_update (m_morph_plan);
    mp_synth.apply_update (update);
  }

  MorphOutputModule *om = mp_synth.voice(0)->output();
  if (om)
    {
      TimeInfo ti; // not relevant
      om->retrigger (ti, 0, 440, 1);
      float s;
      float *values[1] = { &s };
      om->process (ti, 1, values, 1);
    }

  MorphPlanSynth::UpdateP update = m_midi_synth->prepare_update (m_morph_plan);
  m_synth_interface->emit_apply_update (update);
}

void
Project::on_operator_added (MorphOperator *op)
{
  string type = op->type();

  if (type == "SpectMorph::MorphWavSource")
    {
      MorphWavSource *wav_source = static_cast<MorphWavSource *> (op);

      /*
       * only MorphWavSource objects which have been newly created using the UI have object_id == 0
       */
      if (wav_source->object_id() == 0)
        {
          /* load default instrument */
          Instrument *instrument = get_instrument (wav_source);

          Error error = instrument->load (m_user_instrument_index.filename (wav_source->instrument()));
          rebuild (wav_source);
        }
    }
}

void
Project::on_operator_removed (MorphOperator *op)
{
  string type = op->type();

  if (type == "SpectMorph::MorphWavSource")
    {
      MorphWavSource *wav_source = static_cast<MorphWavSource *> (op);

      const int object_id = wav_source->object_id();
      if (object_id)
        {
          /* free instrument data */
          instrument_map[object_id].reset (nullptr);

          /* stop rebuild jobs (if any) */
          m_builder_thread.kill_jobs_by_id (object_id);
          synth_interface()->emit_add_rebuild_result (object_id, nullptr);
        }
    }
}

bool
Project::voices_active()
{
  std::lock_guard<std::mutex> lg (m_synth_mutex);
  return m_voices_active;
}

MorphPlanPtr
Project::morph_plan() const
{
  return m_morph_plan;
}

UserInstrumentIndex *
Project::user_instrument_index()
{
  return &m_user_instrument_index;
}

double
Project::volume() const
{
  return m_volume;
}

void
Project::set_volume (double volume)
{
  m_volume = volume;
  m_synth_interface->emit_update_gain (db_to_factor (m_volume));

  signal_volume_changed (m_volume);
}

vector<MorphWavSource *>
Project::list_wav_sources()
{
  vector<MorphWavSource *> wav_sources;

  // find instrument ids
  for (auto op : m_morph_plan->operators())
    {
      string type = op->type();

      if (type == "SpectMorph::MorphWavSource")
        wav_sources.push_back (static_cast<MorphWavSource *> (op));
    }
  return wav_sources;
}

void
Project::post_load()
{
  clear_lv2_filenames();

  m_builder_thread.kill_all_jobs();
  synth_interface()->emit_clear_wav_sets();
  for (auto wav_source : list_wav_sources())
    rebuild (wav_source);

  // plan has changed due to instrument map initialization:
  //  -> rebuild morph plan view (somewhat hacky)
  m_morph_plan->signal_need_view_rebuild();
  m_morph_plan->emit_plan_changed();
}

Error
Project::load (const string& filename)
{
  if (ZipReader::is_zip (filename))
    {
      ZipReader zip_reader (filename);
      if (zip_reader.error())
        return zip_reader.error();

      return load (zip_reader, nullptr);
    }
  else
    {
      GenericIn *file = GenericIn::open (filename);
      if (file)
        {
          Error error = load_compat (file, nullptr);
          delete file;

          return error;
        }
      else
        {
          return Error::Code::FILE_NOT_FOUND;
        }
    }
}

Error
Project::load (ZipReader& zip_reader, MorphPlan::ExtraParameters *params)
{
  /* backup old plan */
  vector<unsigned char> data;
  MemOut mo (&data);
  m_morph_plan->save (&mo);

  /* backup old instruments */
  map<int, std::unique_ptr<Instrument>> old_instrument_map;
  old_instrument_map.swap (instrument_map);

  Error error = load_internal (zip_reader, params);
  if (error)
    {
      /* restore old plan/instruments if something went wrong */
      GenericIn *old_in = MMapIn::open_mem (&data[0], &data[data.size()]);
      m_morph_plan->load (old_in);
      delete old_in;

      instrument_map.swap (old_instrument_map);
    }
  return error;
}

Error
Project::load_internal (ZipReader& zip_reader, MorphPlan::ExtraParameters *params)
{
  vector<uint8_t> plan = zip_reader.read ("plan.smplan");
  if (zip_reader.error())
    return Error ("Unable to read 'plan.smplan' from input file");

  GenericIn *in = MMapIn::open_mem (&plan[0], &plan[plan.size()]);
  Error error = m_morph_plan->load (in, params);
  delete in;

  if (error)
    return error;

  for (auto wav_source : list_wav_sources())
    {
      const int object_id = wav_source->object_id();

      Instrument *inst = new Instrument();
      instrument_map[object_id].reset (inst);

      if (m_storage_model == StorageModel::COPY)
        {
          string inst_file = string_printf ("instrument%d.sminst", object_id);
          vector<uint8_t> inst_data = zip_reader.read (inst_file);
          if (zip_reader.error())
            return Error (string_printf ("Unable to read '%s' from input file", inst_file.c_str()));

          ZipReader inst_zip (inst_data);
          if (inst_zip.error())
            return inst_zip.error();

          error = inst->load (inst_zip);
          if (error)
            return error;
        }
      else
        {
          inst->load (m_user_instrument_index.filename (wav_source->instrument())); /* ignore errors */
        }
    }

  /* only trigger rebuilds if we loaded everything without error */
  post_load();

  return Error::Code::NONE;
}

Error
Project::load_compat (GenericIn *in, MorphPlan::ExtraParameters *params)
{
  Error error = m_morph_plan->load (in, params);

  if (!error)
    {
      instrument_map.clear();
      post_load();
    }

  return error;
}

void
Project::load_plan_lv2 (std::function<string(string)> absolute_path, const string& plan_str)
{
  // we return silently if string decode or plan load fail:
  //  -> as LV2 plugin we can't really do much if things go wrong

  vector<unsigned char> data;
  if (!HexString::decode (plan_str, data))
    return;

  GenericIn *in = MMapIn::open_mem (&data[0], &data[data.size()]);
  Error error = load_compat (in, nullptr);
  delete in;

  if (error)
    return;

  // LV2 doesn't include instruments
  for (auto wav_source : list_wav_sources())
    {
      const int object_id = wav_source->object_id();

      Instrument *inst = new Instrument();

      // try load mapped path; if this fails, try user instrument dir
      Error error = inst->load (absolute_path (wav_source->lv2_filename()));
      if (error)
        error = inst->load (m_user_instrument_index.filename (wav_source->instrument()));

      // ignore error (if any): we still load preset if instrument is missing
      instrument_map[object_id].reset (inst);
    }
  post_load();
}

Error
Project::save (const std::string& filename)
{
  ZipWriter zip_writer (filename);

  return save (zip_writer, nullptr);
}

Error
Project::save (ZipWriter& zip_writer, MorphPlan::ExtraParameters *params)
{
  vector<unsigned char> data;
  MemOut mo (&data);
  m_morph_plan->save (&mo, params);

  zip_writer.add ("plan.smplan", data);
  for (auto wav_source : list_wav_sources())
    {
      // must do this before using object_id (lazy creation)
      Instrument *instrument = get_instrument (wav_source);

      int    object_id = wav_source->object_id();
      string inst_file = string_printf ("instrument%d.sminst", object_id);

      ZipWriter   mem_zip;
      instrument->save (mem_zip);
      zip_writer.add (inst_file, mem_zip.data(), ZipWriter::Compress::STORE);
    }

  zip_writer.close();
  if (zip_writer.error())
    return zip_writer.error();

  return Error::Code::NONE;
}

string
Project::save_plan_lv2 (std::function<string(string)> abstract_path)
{
  for (auto wav_source : list_wav_sources())
    {
      string lv2_filename = abstract_path (m_user_instrument_index.filename (wav_source->instrument()));
      wav_source->set_lv2_filename (lv2_filename);
    }

  vector<unsigned char> data;
  MemOut mo (&data);
  m_morph_plan->save (&mo);

  clear_lv2_filenames();

  string plan_str = HexString::encode (data);
  return plan_str;
}

void
Project::clear_lv2_filenames()
{
  /* lv2 filenames should only be set for the morph plan saved during lv2 save */
  for (auto wav_source : list_wav_sources())
    wav_source->set_lv2_filename ("");
}
