// Licensed GNU LGPL v2.1 or later: http://www.gnu.org/licenses/lgpl-2.1.html

#include <assert.h>
#include <stdio.h>

#include <vector>
#include <string>

#include "smmain.hh"
#include "smnavigatorwindow.hh"

#include <QApplication>

using namespace SpectMorph;

int
main (int argc, char **argv)
{
  Main main (&argc, &argv);

  QApplication app (argc, argv);

  const char *index;
  if (argc == 1)
    index = "instruments:standard";
  else if (argc == 2)
    index = argv[1];
  if (argc > 2)
    {
      printf ("usage: %s <smindex-file>\n", argv[0]);
      exit (1);
    }

  NavigatorWindow window (index);
  window.show();

  return app.exec();
}
