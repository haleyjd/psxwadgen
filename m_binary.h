// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2013 James Haley
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//   Functions to deal with retrieving data from raw binary.
//
//-----------------------------------------------------------------------------

#ifndef M_BINARY_H__
#define M_BINARY_H__

#include "doomtype.h"

// haleyjd 10/30/10: Read a little-endian short without alignment assumptions
#define read16_le(b, t) ((b)[0] | ((t)((b)[1]) << 8))

// haleyjd 10/30/10: Read a little-endian dword without alignment assumptions
#define read32_le(b, t)   \
   ((b)[0] |              \
    ((t)((b)[1]) <<  8) | \
    ((t)((b)[2]) << 16) | \
    ((t)((b)[3]) << 24))

//
// Reads an int16 from the lump data and increments the read pointer.
//
inline int16_t GetBinaryWord(byte **data)
{
   int16_t val = read16_le(*data, int16_t);
   *data += 2;

   return val;
}

//
// Writes an int16 to the data and increments the write pointer
//
inline void PutBinaryWord(byte *&data, int16_t d)
{
   *data++ = (byte)(d & 0xFF);
   *data++ = (byte)((d >> 8) & 0xFF);
}

//
// Reads a uint16 from the lump data and increments the read pointer.
//
inline uint16_t GetBinaryUWord(byte **data)
{
   uint16_t val = read16_le(*data, uint16_t);
   *data += 2;

   return val;
}

//
// Writes a uint16 to the data and increments the write pointer
//
inline void PutBinaryUWord(byte *&data, uint16_t d)
{
   *data++ = (byte)(d & 0xFF);
   *data++ = (byte)((d >> 8) & 0xFF);
}

//
// Reads an int32 from the lump data and increments the read pointer.
//
inline int32_t GetBinaryDWord(byte **data)
{
   int32_t val = read32_le(*data, int32_t);
   *data += 4;

   return val;
}

//
// Writes an int32 to the data and increments the write pointer
//
inline void PutBinaryDWord(byte *&data, int32_t d)
{
   *data++ = (byte)(d & 0xFF);
   *data++ = (byte)((d >>  8) & 0xFF);
   *data++ = (byte)((d >> 16) & 0xFF);
   *data++ = (byte)((d >> 24) & 0xFF);
}

//
// Reads a uint32 from the lump data and increments the read pointer.
//
inline uint32_t GetBinaryUDWord(byte **data)
{
   uint32_t val = read32_le(*data, uint32_t);
   *data += 4;

   return val;
}

//
// Writes an int32 to the data and increments the write pointer
//
inline void PutBinaryUDWord(byte *&data, uint32_t d)
{
   *data++ = (byte)(d & 0xFF);
   *data++ = (byte)((d >>  8) & 0xFF);
   *data++ = (byte)((d >> 16) & 0xFF);
   *data++ = (byte)((d >> 24) & 0xFF);
}

//
// Reads a "len"-byte string from the lump data and writes it into the 
// destination buffer. The read pointer is incremented by len bytes.
//
inline void GetBinaryString(byte **data, char *dest, int len)
{
   char *loc = (char *)(*data);

   memcpy(dest, loc, len);

   *data += len;
}

//
// Writes a "len"-byte string into the destination buffer. The write pointer
// is incremented by len bytes.
//
inline void PutBinaryString(byte *&data, const char *src, int len)
{
    memcpy(data, src, len);
    data += len;
}

//
// Write the char array into the destination buffer. The write pointer is
// incremented by the size of the array.
//
template<size_t N>
inline void PutBinaryCharArray(byte *&data, const char (&src)[N])
{
    memcpy(data, src, N);
    data += N;
}

#endif

// EOF

