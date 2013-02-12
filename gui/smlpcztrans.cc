// Licensed GNU LGPL v3 or later: http://www.gnu.org/licenses/lgpl.html

#include <bse/bsemathsignal.hh>
#include <complex>
#include <vector>
#include "smlpcztrans.hh"

using namespace SpectMorph;

using std::vector;
using std::complex;

#if 0
GdkPixbuf *
SpectMorph::lpc_z_transform (const LPCZFunction& zfunc, const vector< complex<double> >& roots)
{
  const size_t width = 1000, height = 1000;

  GdkPixbuf *pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, /* has_alpha */ false, 8, width, height);
  uint row_stride = gdk_pixbuf_get_rowstride (pixbuf);

  for (size_t y = 0; y < width; y++)
    {
      for (size_t x = 0; x < width; x++)
        {
          double re = double (x * 2) / width - 1;
          double im = double (y * 2) / height - 1;
          guchar *p = gdk_pixbuf_get_pixels (pixbuf) + 3 * x + y * row_stride;

          complex<double> z (re, im);
          double value = zfunc.eval (z);
          double db = bse_db_from_factor (value, -200);
          db += 150;

          int idb = CLAMP (db, 0, 255);
          p[0] = idb;
          p[1] = idb;
          p[2] = idb;
        }
    }
  for (size_t i = 0; i < roots.size(); i++)
    {
      int x = (roots[i].real() + 1) * width * 0.5;
      int y = (roots[i].imag() + 1) * height * 0.5;
      if (x >= 3 && x < int (width - 3) && y >= 3 && y < int (height - 3))
        {
          guchar *p = gdk_pixbuf_get_pixels (pixbuf) + 3 * x + y * row_stride;
          int down_right = row_stride + 3;
          int down_left = row_stride - 3;
          for (int delta = -3; delta <= 3; delta++)
            {
              p[0 + delta * down_right] = p[0 + delta * down_left] = 255;
              p[1 + delta * down_right] = p[1 + delta * down_left] = 0;
              p[2 + delta * down_right] = p[2 + delta * down_left] = 0;
            }
        }
    }
  return pixbuf;
}

GdkPixbuf *
SpectMorph::lpc_z_transform (const vector<double>& a, const vector< complex<double> >& roots)
{
  return lpc_z_transform (LPCZFunctionLPC (a), roots);
}
#endif
