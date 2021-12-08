// Licensed GNU LGPL v2.1 or later: http://www.gnu.org/licenses/lgpl-2.1.html

#include "smmain.hh"
#include "smutils.hh"
#include <string>
#include <vector>

using std::string;
using std::vector;
using namespace SpectMorph;

/// @cond
struct Options
{
  string	 program_name; /* FIXME: what to do with that */
  string         config_filename;
  vector<string> args;

  Options ();
  void parse (int *argc_p, char **argv_p[]);
  static void print_usage ();
} options;
/// @endcond

#include "stwutils.hh"

Options::Options()
{
}

void
Options::parse (int   *argc_p,
                char **argv_p[])
{
  guint argc = *argc_p;
  gchar **argv = *argv_p;
  unsigned int i, e;

  for (i = 1; i < argc; i++)
    {
      const char *opt_arg;
      if (strcmp (argv[i], "--help") == 0 ||
          strcmp (argv[i], "-h") == 0)
	{
	  print_usage();
	  exit (0);
	}
      else if (check_arg (argc, argv, &i, "-d"))
	{
          args.push_back ("-d");
	}
      else if (check_arg (argc, argv, &i, "-f", &opt_arg))
	{
          args.push_back (string_printf ("-f %s", opt_arg));
        }
      else if (check_arg (argc, argv, &i, "-m", &opt_arg))
        {
          args.push_back (string_printf ("-m %s", opt_arg));
        }
      else if (check_arg (argc, argv, &i, "-O0"))
        {
          args.push_back ("-O0");
        }
      else if (check_arg (argc, argv, &i, "-O1"))
        {
          args.push_back ("-O1");
        }
      else if (check_arg (argc, argv, &i, "-O2"))
        {
          args.push_back ("-O2");
        }
      else if (check_arg (argc, argv, &i, "-O", &opt_arg))
        {
          args.push_back (string_printf ("-O %s", opt_arg));
        }
      else if (check_arg (argc, argv, &i, "-s"))
        {
          args.push_back ("-s");
        }
      else if (check_arg (argc, argv, &i, "--keep-samples"))
        {
          args.push_back ("--keep-samples");
        }
      else if (check_arg (argc, argv, &i, "--no-attack"))
        {
          args.push_back ("--no-attack");
        }
      else if (check_arg (argc, argv, &i, "--no-sines"))
        {
          args.push_back ("--no-sines");
        }
      else if (check_arg (argc, argv, &i, "--loop-start", &opt_arg))
        {
          args.push_back (string_printf ("--loop-start %s", opt_arg));
        }
      else if (check_arg (argc, argv, &i, "--loop-end", &opt_arg))
        {
          args.push_back (string_printf ("--loop-end %s", opt_arg));
        }
      else if (check_arg (argc, argv, &i, "--loop-type", &opt_arg))
        {
          args.push_back (string_printf ("--loop-type %s", opt_arg));
        }
      else if (check_arg (argc, argv, &i, "--loop-unit", &opt_arg))
        {
          args.push_back (string_printf ("--loop-unit %s", opt_arg));
        }
      else if (check_arg (argc, argv, &i, "--config", &opt_arg))
        {
          args.push_back (string_printf ("--config %s", opt_arg));
          config_filename = opt_arg;
        }
     }

  /* resort argc/argv */
  e = 1;
  for (i = 1; i < argc; i++)
    if (argv[i])
      {
        argv[e++] = argv[i];
        if (i >= e)
          argv[i] = NULL;
      }
  *argc_p = e;
}

void
Options::print_usage ()
{
  printf ("usage: %s [ <options> ] <src_audio_file> [ <dest_sm_file> ]\n", options.program_name.c_str());
  printf ("\n");
  printf ("options:\n");
  printf (" -h, --help                  help for %s\n", options.program_name.c_str());
  printf (" --version                   print version\n");
  printf (" -f <freq>                   specify fundamental frequency in Hz\n");
  printf (" -m <note>                   specify midi note for fundamental frequency\n");
  printf (" -O <level>                  set optimization level\n");
  printf (" -s                          produced stripped models\n");
  printf (" --no-attack                 skip attack time optimization\n");
  printf (" --no-sines                  skip partial tracking\n");
  printf (" --loop-start                set timeloop start\n");
  printf (" --loop-end                  set timeloop end\n");
  printf ("\n");
}


// smenc -m 84 clipped-note-84.wav /home/stefan/src/diplom/evaluation/violin/data/violin/84.sm -O1

void
die (const string& reason)
{
  printf ("smenccache: %s\n", reason.c_str());
  exit (1);
}

int
main (int argc, char **argv)
{
  Main main (&argc, &argv);
  options.parse (&argc, &argv);

  if (argc == 3)
    {
      string cmdline = string_printf ("%s/smenc \"%s\" \"%s\"", sm_get_install_dir (INSTALL_DIR_BIN).c_str(), argv[1], argv[2]);
      string cmdargs;
      for (vector<string>::iterator ai = options.args.begin(); ai != options.args.end(); ai++)
        cmdargs += " " + *ai;
      cmdline += cmdargs;

      /* hash commandline args */
      vector<unsigned char> data;
      for (size_t i = 0; i < cmdargs.size(); i++)
        data.push_back (cmdargs[i]);
      data.push_back (0);

      int ch;
      /* hash contents of config file */
      if (options.config_filename != "")
        {
          FILE *config_file = fopen (options.config_filename.c_str(), "r");
          if (!config_file)
            die ("config file not found");
          while ((ch = fgetc (config_file)) >= 0)
            data.push_back (ch);
        }
      data.push_back (0);

      /* hash contents of infile */
      FILE *infile = fopen (argv[1], "r");
      if (!infile)
        die ("no infile");

      while ((ch = fgetc (infile)) >= 0)
        data.push_back (ch);

      char *sha256_sum = g_compute_checksum_for_data (G_CHECKSUM_SHA256, &data[0], data.size());

      string cache_dir = sm_get_cache_dir() + "/smenccache";
      g_mkdir_with_parents (cache_dir.c_str(), 0775);

      string cache_filename = string_printf ("%s/%s", cache_dir.c_str(), sha256_sum);
      FILE *cache_file = fopen (cache_filename.c_str(), "r");
      if (cache_file)
        {
          string cpcmd = string_printf ("cp %s %s", cache_filename.c_str(), argv[2]);
          int cret = system (cpcmd.c_str());
          int cxstatus = WEXITSTATUS (cret);
          exit (cxstatus);
        }
      else
        {
          int ret = system (cmdline.c_str());

          int xstatus = WEXITSTATUS (ret);
          if (xstatus == 0)
            {
              string cpcmd = string_printf ("cp %s %s", argv[2], cache_filename.c_str());
              int cret = system (cpcmd.c_str());
              int cxstatus = WEXITSTATUS (cret);
              exit (cxstatus);
            }
          else
            {
              exit (xstatus);
            }
        }

      g_free (sha256_sum);
    }
  string remaining = "";
  for (int i = 1; i < argc; i++)
    {
      if (i != 1)
        remaining += " ";
      remaining += argv[i];
    }
  die ("bad args: " + remaining);
}
