/*
 * Copyright (C) 2010 Stefan Westerfeld
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "smmmapin.hh"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

using SpectMorph::MMapIn;
using SpectMorph::GenericIn;

GenericIn*
MMapIn::open (const std::string& filename)
{
  int fd = ::open (filename.c_str(), O_RDONLY);
  if (fd >= 0)
    {
      struct stat st;

      if (fstat (fd, &st) == 0)
        {
          unsigned char *mapfile = static_cast<unsigned char *> (mmap (NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0));
          if (mapfile)
            {
              return new MMapIn (mapfile, mapfile + st.st_size);
            }
        }
    }
  return NULL;
}

MMapIn::MMapIn (unsigned char *mapfile, unsigned char *mapend) :
  mapfile (mapfile),
  mapend (mapend)
{
  pos = static_cast<unsigned char *> (mapfile);
}

int
MMapIn::get_byte()
{
  if (pos < mapend)
    return *pos++;
  else
    return EOF;
}

int
MMapIn::read (void *ptr, size_t size)
{
  if (pos + size <= mapend)
    {
      memcpy (ptr, pos, size);
      pos += size;
      return size;
    }
  else
    return 0;
}

int
MMapIn::seek (long offset, int whence)
{
  assert (whence == SEEK_CUR);
  if (pos + offset <= mapend)
    {
      pos += offset;
      return 0;
    }
  else
    {
      return -1;
    }
}

unsigned char *
MMapIn::mmap_mem (size_t& remaining)
{
  remaining = mapend - pos;
  return pos;
}

size_t
MMapIn::get_pos()
{
  return pos - mapfile;
}

GenericIn *
MMapIn::open_subfile (size_t pos, size_t len)
{
  return new MMapIn (mapfile + pos, mapfile + pos + len);
}
