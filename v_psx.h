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
//   PSX Graphics Formats
//
//-----------------------------------------------------------------------------

#ifndef V_PSX_H__
#define V_PSX_H__

#include "z_zone.h"

class qstring;
class WadDirectory;
struct ziparchive_t;

struct rgba_t
{
   uint8_t r, g, b, a;
};

//
// VPSXImage
//
// Class to encapsulate image data extracted from a PSX graphic lump.
//
class VPSXImage : public ZoneObject
{
protected:
   int16_t top;
   int16_t left;
   int16_t width;
   int16_t height;

   uint8_t *pixels;
   uint8_t *mask;

   void readImage(const void *data);
   void adjustOffsets(const char *name);

public:
   VPSXImage(WadDirectory &dir, int lumpnum);
   VPSXImage(WadDirectory &dir, const char *lumpname);

   int16_t getTop()    const { return top;    }
   int16_t getLeft()   const { return left;   }
   int16_t getWidth()  const { return width;  }
   int16_t getHeight() const { return height; }
   
   const uint8_t *getPixels() const { return pixels; }
   const uint8_t *getMask()   const { return mask;   }

   void *toPatch(size_t &size) const;
};

// Known PSX PLAYPAL palette numbers
enum psxpalette_e
{
   PAL_NORMAL,
   PAL_DAMAGE1,
   PAL_DAMAGE2,
   PAL_DAMAGE3,
   PAL_DAMAGE4,
   PAL_DAMAGE5,
   PAL_DAMAGE6,
   PAL_DAMAGE7,
   PAL_DAMAGE8,
   PAL_ITEM1,
   PAL_ITEM2,
   PAL_ITEM3,
   PAL_ITEM4,
   PAL_RADSUIT,
   PAL_GODMODE,    // Used for godmode instead of a colormap
   PAL_FIRESKY,    // Used for realtime fire sky effect
   PAL_LEGALS,     // Used for LEGALS lump
   PAL_DOOMTITLE,  // Used for DOOM and TITLE lumps
   PAL_IDCRED1,    // Used for IDCRED1 lump
   PAL_WMSCRED1,   // Used for WMSCRED1 lump
   PAL_NUMPLAYPALS
};

void V_LoadPLAYPAL(WadDirectory &dir);
void V_GenerateCOLORMAP();

void V_ConvertSpritesToZip(WadDirectory &dir, ziparchive_t *zip);
void V_ConvertTexturesToZip(WadDirectory &dir, ziparchive_t *zip);
void V_ConvertFlatsToZip(WadDirectory &dir, ziparchive_t *zip);
void V_ConvertPLAYPALToZip(ziparchive_t *zip);
void V_ConvertCOLORMAPToZip(ziparchive_t *zip);

void V_ExtractMovie(const qstring &infile, const qstring &outfile, 
                    int offset, int length);

#ifndef NO_UNIT_TESTS
void V_ExplodePLAYPAL(WadDirectory &dir);
#endif

#endif

// EOF

