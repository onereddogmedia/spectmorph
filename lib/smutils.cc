// Licensed GNU LGPL v3 or later: http://www.gnu.org/licenses/lgpl.html

#include "smutils.hh"

#include <string>
#include <assert.h>

/********************************************************************************
* START: Code from Rapicorn
*********************************************************************************/

namespace {

// == locale ==

/// Class to push a specific locale_t for the scope of its lifetime.
class ScopedLocale {
  locale_t      locale_;
  /*copy*/      ScopedLocale (const ScopedLocale&) = delete;
  ScopedLocale& operator=    (const ScopedLocale&) = delete;
protected:
  explicit      ScopedLocale (locale_t scope_locale);
public:
  // explicit   ScopedLocale (const String &locale_name = ""); // not supported
  /*dtor*/     ~ScopedLocale ();
};

/// Class to push the POSIX/C locale_t (UTF-8) for the scope of its lifetime.
class ScopedPosixLocale : public ScopedLocale {
public:
  explicit        ScopedPosixLocale ();
  static locale_t posix_locale      (); ///< Retrieve the (UTF-8) POSIX/C locale_t.
};

ScopedLocale::ScopedLocale (locale_t scope_locale) :
  locale_ (NULL)
{
  if (!scope_locale)
    locale_ = uselocale (LC_GLOBAL_LOCALE);     // use process locale
  else
    locale_ = uselocale (scope_locale);         // use custom locale
  assert (locale_ != NULL);
}

ScopedLocale::~ScopedLocale ()
{
  uselocale (locale_);                          // restore locale
}

#if 0
ScopedLocale::ScopedLocale (const String &locale_name = "")
{
  /* this constructor should:
   * - uselocale (LC_GLOBAL_LOCALE) if locale_name == "",
   * - create newlocale from locale_name, use it and later delete it, but:
   * - freelocale(newlocale()) seems buggy on glibc-2.7 (crashes)
   */
}
#endif

ScopedPosixLocale::ScopedPosixLocale () :
  ScopedLocale (posix_locale())
{}

locale_t
ScopedPosixLocale::posix_locale ()
{
  static locale_t volatile posix_locale_ = NULL;
  if (!posix_locale_)
    {
      locale_t posix_locale = NULL;
      if (!posix_locale)
        posix_locale = newlocale (LC_ALL_MASK, "POSIX.UTF-8", NULL);
      if (!posix_locale)
        posix_locale = newlocale (LC_ALL_MASK, "C.UTF-8", NULL);
      if (!posix_locale)
        posix_locale = newlocale (LC_ALL_MASK, "POSIX", NULL);
      if (!posix_locale)
        posix_locale = newlocale (LC_ALL_MASK, "C", NULL);
      if (!posix_locale)
        posix_locale = newlocale (LC_ALL_MASK, NULL, NULL);
      assert (posix_locale != NULL);
      if (!__sync_bool_compare_and_swap (&posix_locale_, NULL, posix_locale))
        freelocale (posix_locale_);
    }
  return posix_locale_;
}

}

/********************************************************************************
* END: Code from Rapicorn
*********************************************************************************/
using std::string;

static string
string_vprintf (const char *format,
                va_list     vargs)
{
  char *str = NULL;
  if (vasprintf (&str, format, vargs) >= 0 && str)
    {
      string s = str;
      free (str);
      return s;
    }
  else
    return format;
}

namespace SpectMorph
{

string
string_printf (const char *format, ...)
{
  ScopedPosixLocale posix_locale_scope; // pushes POSIX locale for this scope

  string str;
  va_list args;
  va_start (args, format);
  str = string_vprintf (format, args);
  va_end (args);
  return str;
}

void
sm_printf (const char *format, ...)
{
  ScopedPosixLocale posix_locale_scope; // pushes POSIX locale for this scope

  string str;
  va_list args;
  va_start (args, format);
  str = string_vprintf (format, args);
  va_end (args);

  printf ("%s", str.c_str());
}

}
