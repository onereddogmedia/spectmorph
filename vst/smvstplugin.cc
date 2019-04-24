/*
 *  VST Plugin is under GPL 2 or later, since it is based on Vestige (GPL) and amsynth_vst (GPL)
 *
 *  Copyright (c) 2008-2015 Nick Dowell
 *  Copyright (c) 2016 Stefan Westerfeld
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <cassert>
#include <cstdlib>
#include <cstdio>
#include <cstring>

#include "vestige/aeffectx.h"
#include "smutils.hh"
#include "smmorphplan.hh"
#include "smmorphplansynth.hh"
#include "smmidisynth.hh"
#include "smmain.hh"
#include "smvstui.hh"
#include "smvstplugin.hh"
#include "smmorphoutputmodule.hh"

#ifdef SM_OS_MACOS // need to include this before using namespace SpectMorph
#include <CoreFoundation/CoreFoundation.h>
#endif

// from http://www.asseca.org/vst-24-specs/index.html
#define effGetParamLabel        6
#define effGetParamDisplay      7
#define effGetChunk             23
#define effSetChunk             24
#define effCanBeAutomated       26
#define effGetOutputProperties  34
#define effVendorSpecific       50
#define effGetTailSize          52
#define effGetMidiKeyName       66
#define effBeginLoadBank        75
#define effFlagsProgramChunks   (1 << 5)

using namespace SpectMorph;

using std::string;
using std::vector;

VstPlugin::VstPlugin (audioMasterCallback master, AEffect *aeffect) :
  audioMaster (master),
  aeffect (aeffect),
  plan (new MorphPlan (project)),
  midi_synth (nullptr)
{
  audioMaster = master;

  plan->load_default();

  ui = new VstUI (plan->clone(), this);

  parameters.push_back (Parameter ("Control #1", 0, -1, 1));
  parameters.push_back (Parameter ("Control #2", 0, -1, 1));

  set_volume (-6); // default volume

  // initialize mix_freq with something, so that the plugin doesn't crash if the host never calls SetSampleRate
  set_mix_freq (48000);
}

VstPlugin::~VstPlugin()
{
  delete ui;
  ui = nullptr;

  if (midi_synth)
    {
      delete midi_synth;
      midi_synth = nullptr;
    }
}

void
VstPlugin::change_plan (MorphPlanPtr plan)
{
  project.update_plan (plan);
}

void
VstPlugin::set_volume (double new_volume)
{
  std::lock_guard<std::mutex> locker (project.synth_mutex());
  m_volume = new_volume;
}

double
VstPlugin::volume()
{
  std::lock_guard<std::mutex> locker (project.synth_mutex());
  return m_volume;
}

void
VstPlugin::get_parameter_name (Param param, char *out, size_t len) const
{
  if (param >= 0 && param < parameters.size())
    strncpy (out, parameters[param].name.c_str(), len);
}

void
VstPlugin::get_parameter_label (Param param, char *out, size_t len) const
{
  if (param >= 0 && param < parameters.size())
    strncpy (out, parameters[param].label.c_str(), len);
}

void
VstPlugin::get_parameter_display (Param param, char *out, size_t len) const
{
  if (param >= 0 && param < parameters.size())
    strncpy (out, string_printf ("%.5f", parameters[param].value).c_str(), len);
}

void
VstPlugin::set_parameter_scale (Param param, float value)
{
  if (param >= 0 && param < parameters.size())
    parameters[param].value = parameters[param].min_value + (parameters[param].max_value - parameters[param].min_value) * value;
}

float
VstPlugin::get_parameter_scale (Param param) const
{
  if (param >= 0 && param < parameters.size())
    return (parameters[param].value - parameters[param].min_value) / (parameters[param].max_value - parameters[param].min_value);

  return 0;
}

float
VstPlugin::get_parameter_value (Param param) const
{
  if (param >= 0 && param < parameters.size())
    return parameters[param].value;

  return 0;
}

void
VstPlugin::set_parameter_value (Param param, float value)
{
  if (param >= 0 && param < parameters.size())
    parameters[param].value = value;
}

void
VstPlugin::set_mix_freq (double new_mix_freq)
{
  /* this should only be called by the host if the plugin is suspended, so
   * we can alter variables that are used by process|processReplacing in the real time thread
   */
  if (midi_synth)
    delete midi_synth;

  mix_freq = new_mix_freq;

  midi_synth = new MidiSynth (mix_freq, 64);
  midi_synth->update_plan (plan);

  project.change_midi_synth (midi_synth);
}


static char hostProductString[64] = "";

static intptr_t dispatcher(AEffect *effect, int opcode, int index, intptr_t val, void *ptr, float f)
{
  VstPlugin *plugin = (VstPlugin *)effect->ptr3;

  switch (opcode) {
    case effOpen:
      return 0;

    case effClose:
      delete plugin;
      memset(effect, 0, sizeof(AEffect));
      free(effect);
      return 0;

    case effSetProgram:
    case effGetProgram:
    case effGetProgramName:
      return 0;

    case effGetParamLabel:
      plugin->get_parameter_label ((VstPlugin::Param)index, (char *)ptr, 32);
      return 0;

    case effGetParamDisplay:
      plugin->get_parameter_display ((VstPlugin::Param)index, (char *)ptr, 32);
      return 0;

    case effGetParamName:
      plugin->get_parameter_name ((VstPlugin::Param)index, (char *)ptr, 32);
      return 0;

    case effCanBeAutomated:
      // for now, all parameters can be automated, so we hardcode this
      if (index >= 0 && index < VstPlugin::PARAM_COUNT)
        return 1;
      return 0;

    case effSetSampleRate:
      plugin->set_mix_freq (f);
      return 0;

    case effSetBlockSize:
    case effMainsChanged:
      return 0;

    case effEditGetRect:
      plugin->ui->getRect ((ERect **) ptr);
      return 1;

    case effEditOpen:
      plugin->ui->open((PuglNativeWindow)(uintptr_t)ptr);
      return 1;

    case effEditClose:
      plugin->ui->close();
      return 0;

    case effEditIdle:
      plugin->ui->idle();
      return 0;

    case effGetChunk:
      {
        int result = plugin->ui->save_state((char **)ptr);
        VST_DEBUG ("get chunk returned: %s\n", *(char **)ptr);
        return result;
      }

    case effSetChunk:
      VST_DEBUG ("set chunk: %s\n", (char *)ptr);
      plugin->ui->load_state((char *)ptr);
      return 0;

    case effProcessEvents:
      {
        VstEvents *events = (VstEvents *)ptr;

        for (int32_t i = 0; i < events->numEvents; i++)
          {
            VstMidiEvent *event = (VstMidiEvent *)events->events[i];
            if (event->type != kVstMidiType)
              continue;

            plugin->midi_synth->add_midi_event (event->deltaFrames, reinterpret_cast <unsigned char *> (&event->midiData[0]));
          }
        return 1;
      }

    case effGetOutputProperties:
      return 0;

    case effGetPlugCategory:
      return kPlugCategSynth;
    case effGetEffectName:
      strcpy((char *)ptr, "SpectMorph");
      return 1;
    case effGetVendorString:
      strcpy((char *)ptr, "Stefan Westerfeld");
      return 1;
    case effGetProductString:
      strcpy((char *)ptr, "SpectMorph");
      return 1;
    case effGetVendorVersion:
       return 0;
    case effCanDo:
      if (strcmp("receiveVstMidiEvent", (char *)ptr) == 0 ||
          strcmp("MPE", (char *)ptr) == 0 ||
          strcmp ("supportsViewDpiScaling", (char *)ptr) == 0) return 1;
      if (strcmp("midiKeyBasedInstrumentControl", (char *)ptr) == 0 ||
              strcmp("receiveVstSysexEvent", (char *)ptr) == 0 ||
              strcmp("midiSingleNoteTuningChange", (char *)ptr) == 0 ||
              strcmp("sendVstMidiEvent", (char *)ptr) == 0 ||
              false) return 0;
      VST_DEBUG ("unhandled canDo: %s\n", (char *)ptr);
      return 0;

    case effGetTailSize:
    case effIdle:
    case effGetParameterProperties:
      return 0;

    case effGetVstVersion:
      return 2400;

    case effGetMidiKeyName:
    case effStartProcess:
    case effStopProcess:
    case effBeginSetProgram:
    case effEndSetProgram:
    case effBeginLoadBank:
      return 0;

    default:
      VST_DEBUG ("[smvstplugin] unhandled VST opcode: %d\n", opcode);
      return 0;
  }
}

static void
processReplacing (AEffect *effect, float **inputs, float **outputs, int numSampleFrames)
{
  VstPlugin *plugin = (VstPlugin *)effect->ptr3;

  // update plan with new parameters / new modules if necessary
  plugin->project.try_update_synth();

  if (plugin->project.synth_mutex().try_lock())
    {
      plugin->rt_volume = plugin->m_volume;
      plugin->project.synth_mutex().unlock();
    }
  plugin->midi_synth->set_control_input (0, plugin->parameters[VstPlugin::PARAM_CONTROL_1].value);
  plugin->midi_synth->set_control_input (1, plugin->parameters[VstPlugin::PARAM_CONTROL_2].value);
  plugin->midi_synth->process (outputs[0], numSampleFrames);

  // apply replay volume
  const float volume_factor = db_to_factor (plugin->rt_volume);
  for (int i = 0; i < numSampleFrames; i++)
    outputs[0][i] *= volume_factor;

  std::copy (outputs[0], outputs[0] + numSampleFrames, outputs[1]);
}

static void
process (AEffect *effect, float **inputs, float **outputs, int numSampleFrames)
{
  // this is not as efficient as it could be
  // however, most likely the actual morphing is a lot more expensive

  float  tmp_output_l[numSampleFrames];
  float  tmp_output_r[numSampleFrames];
  float *tmp_outputs[2] = { tmp_output_l, tmp_output_r };

  processReplacing (effect, inputs, tmp_outputs, numSampleFrames);

  for (int i = 0; i < numSampleFrames; i++)
    {
      outputs[0][i] += tmp_output_l[i];
      outputs[1][i] += tmp_output_r[i];
    }
}

static void setParameter(AEffect *effect, int i, float f)
{
  VstPlugin *plugin = (VstPlugin *)effect->ptr3;

  plugin->set_parameter_scale ((VstPlugin::Param) i, f);
}

static float getParameter(AEffect *effect, int i)
{
  VstPlugin *plugin = (VstPlugin *)effect->ptr3;

  return plugin->get_parameter_scale((VstPlugin::Param) i);
}

#ifdef SM_OS_MACOS
static void
set_macos_data_dir()
{
  CFBundleRef ref = CFBundleGetBundleWithIdentifier (CFSTR ("org.spectmorph.vst.SpectMorph"));

  if(ref)
    {
      CFURLRef url = CFBundleCopyBundleURL (ref);
      if (url)
        {
          char path[1024];

          CFURLGetFileSystemRepresentation (url, true, (UInt8 *) path, 1024);
          CFRelease (url);

          VST_DEBUG ("macOS bundle path: '%s'\n", path);

          string pkg_data_dir = path;
          pkg_data_dir += "/Contents/Resources";

          VST_DEBUG ("pkg data dir: '%s'\n", pkg_data_dir.c_str());
          sm_set_pkg_data_dir (pkg_data_dir);
        }
    }
}
#endif

#ifdef SM_OS_WINDOWS
#include "windows.h"

HMODULE hInstance;

extern "C" {
BOOL WINAPI DllMain (HINSTANCE hInst, DWORD dwReason, LPVOID lpvReserved)
{
  hInstance = hInst;
  return 1;
}
} // extern "C"

static void
set_windows_data_dir()
{
  char path[MAX_PATH];

  if (!GetModuleFileName (hInstance, path, MAX_PATH))
    {
      VST_DEBUG ("windows data dir: GetModuleFileName failed\n");
      return;
    }
  VST_DEBUG ("windows data dir: dll path is '%s'\n", path);

  char *last_backslash = strrchr (path, '\\');
  if (!last_backslash)
    {
      VST_DEBUG ("windows data dir: no backslash found\n");
      return;
    }
  *last_backslash = 0;

  string link = string (path) + "\\SpectMorph.data.lnk";
  string pkg_data_dir = sm_resolve_link (link);
  if (pkg_data_dir == "")
    {
      VST_DEBUG ("windows data dir: error resolving link '%s'\n", link.c_str());
      return;
    }

  VST_DEBUG ("windows data dir: link points to '%s'\n", pkg_data_dir.c_str());
  sm_set_pkg_data_dir (pkg_data_dir);
}

extern "C" AEffect *VSTPluginMain (audioMasterCallback audioMaster) __declspec(dllexport);
#else
extern "C" AEffect *VSTPluginMain (audioMasterCallback audioMaster) __attribute__((visibility("default")));
#endif

extern "C" AEffect *VSTPluginMain (audioMasterCallback audioMaster)
{
  Debug::set_filename ("smvstplugin.log");

  if (!sm_init_done())
    sm_init_plugin();

  VST_DEBUG ("VSTPluginMain called\n"); // debug statements are only visible after init

  if (audioMaster)
    {
      audioMaster (NULL, audioMasterGetProductString, 0, 0, hostProductString, 0.0f);
      VST_DEBUG ("Host: %s\n", hostProductString);
    }

#ifdef SM_OS_WINDOWS
  set_windows_data_dir();
#endif
#ifdef SM_OS_MACOS
  set_macos_data_dir();
#endif

  AEffect *effect = (AEffect *)calloc(1, sizeof(AEffect));
  effect->magic = kEffectMagic;
  effect->dispatcher = dispatcher;
  effect->process = process;
  effect->setParameter = setParameter;
  effect->getParameter = getParameter;
  effect->numPrograms = 0;
  effect->numParams = VstPlugin::PARAM_COUNT;
  effect->numInputs = 0;
  effect->numOutputs = 2;
  effect->flags = effFlagsCanReplacing | effFlagsIsSynth | effFlagsProgramChunks | effFlagsHasEditor;

  // Do no use the ->user pointer because ardour clobbers it
  effect->ptr3 = new VstPlugin (audioMaster, effect);
  effect->uniqueID = CCONST ('s', 'm', 'r', 'p');
  effect->processReplacing = processReplacing;

  VST_DEBUG ("VSTPluginMain done => return %p\n", effect);
  return effect;
}

#if 0
// this is required because GCC throws an error if we declare a non-standard function named 'main'
extern "C" __attribute__ ((visibility("default"))) AEffect * main_plugin(audioMasterCallback audioMaster) asm ("main");

extern "C" __attribute__ ((visibility("default"))) AEffect * main_plugin(audioMasterCallback audioMaster)
{
	return VSTPluginMain (audioMaster);
}
#endif
