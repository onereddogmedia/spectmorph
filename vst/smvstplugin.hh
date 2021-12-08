// Licensed GNU LGPL v2.1 or later: http://www.gnu.org/licenses/lgpl-2.1.html

#ifndef SPECTMORPH_VST_PLUGIN_HH
#define SPECTMORPH_VST_PLUGIN_HH

#include "vestige/aeffectx.h"
#include "smmorphplansynth.hh"
#include "smmidisynth.hh"

#define VST_DEBUG(...) Debug::debug ("vst", __VA_ARGS__)

namespace SpectMorph
{

struct VstPlugin
{
  enum Param
  {
    PARAM_CONTROL_1 = 0,
    PARAM_CONTROL_2 = 1,
    PARAM_CONTROL_3 = 2,
    PARAM_CONTROL_4 = 3,
    PARAM_COUNT
  };

  struct Parameter
  {
    std::string name;
    float       value;
    float       min_value;
    float       max_value;
    std::string label;

    Parameter (const char *name, float default_value, float min_value, float max_value, std::string label = "") :
      name (name),
      value (default_value),
      min_value (min_value),
      max_value (max_value),
      label (label)
    {
    }
  };
  std::vector<Parameter> parameters;
  std::vector<uint8_t>   chunk_data;

  VstPlugin (audioMasterCallback master, AEffect *aeffect);
  ~VstPlugin();

  void  get_parameter_name (Param param, char *out, size_t len) const;
  void  get_parameter_label (Param param, char *out, size_t len) const;
  void  get_parameter_display (Param param, char *out, size_t len) const;

  float get_parameter_scale (Param param) const;
  void  set_parameter_scale (Param param, float value);

  float get_parameter_value (Param param) const;
  void  set_parameter_value (Param param, float value);

  bool  voices_active();

  void  set_mix_freq (double mix_freq);

  int   save_state (char **ptr);
  void  load_state (char *ptr, size_t size);

  audioMasterCallback audioMaster;
  AEffect*            aeffect;

  Project             project;
  VstUI              *ui;
};

}

#endif
