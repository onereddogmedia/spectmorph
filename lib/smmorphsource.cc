// Licensed GNU LGPL v2.1 or later: http://www.gnu.org/licenses/lgpl-2.1.html

#include "smmorphsource.hh"
#include "smmorphplan.hh"
#include "smleakdebugger.hh"

using namespace SpectMorph;

using std::string;

static LeakDebugger leak_debugger ("SpectMorph::MorphSource");

MorphSource::MorphSource (MorphPlan *morph_plan) :
  MorphOperator (morph_plan)
{
  leak_debugger.add (this);
}

MorphSource::~MorphSource()
{
  leak_debugger.del (this);
}

void
MorphSource::set_smset (const string& smset)
{
  m_smset = smset;
  m_morph_plan->emit_plan_changed();
}

string
MorphSource::smset()
{
  return m_smset;
}

MorphOperatorConfig *
MorphSource::clone_config()
{
  Config *cfg = new Config (m_config);

  string smset_dir = morph_plan()->index()->smset_dir();
  cfg->path = smset_dir + "/" + m_smset;

  return cfg;
}

const char *
MorphSource::type()
{
  return "SpectMorph::MorphSource";
}

int
MorphSource::insert_order()
{
  return 0;
}

bool
MorphSource::save (OutFile& out_file)
{
  out_file.write_string ("instrument", m_smset);

  return true;
}

bool
MorphSource::load (InFile& ifile)
{
  while (ifile.event() != InFile::END_OF_FILE)
    {
      if (ifile.event() == InFile::STRING)
        {
          if (ifile.event_name() == "instrument")
            {
              m_smset = ifile.event_data();
            }
          else
            {
              g_printerr ("bad string\n");
              return false;
            }
        }
      else
        {
          g_printerr ("bad event\n");
          return false;
        }
      ifile.next_event();
    }
  return true;
}

MorphOperator::OutputType
MorphSource::output_type()
{
  return OUTPUT_AUDIO;
}
