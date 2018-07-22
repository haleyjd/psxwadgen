//
// Copyright(C) 2018 James Haley
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

#ifndef D_LEVEL_H__
#define D_LEVEL_H__

class WadDirectory;
class qstring;

// Blend modes
enum psxblendmode_e
{
   PBM_TL50,      // 50% transparent;  B/2 + F/2
   PBM_TLADD100,  // 100% additive;    B + F
   PBM_NIGHTMARE, // 100% subtractive; B - F
   PBM_TLADD25    // 25% additive;     B + F/4
};

// Thing flags
enum mapthingflags_e
{
   MTF_EASY          = 1,
   MTF_NORMAL        = 2,
   MTF_HARD          = 4,
   MTF_AMBUSH        = 8,
   MTF_MULTIPLAYER   = 16,
   MTF_PSX_USEBLEND  = 32,
   MTF_PSX_BLENDMASK = (64|128),
   
   MTF_BLENDSHIFT    = 6
};

// Thing definition, position, orientation and type,
// plus skill/visibility flags and attributes.
struct mapthing_t
{
   int16_t x;
   int16_t y;
   int16_t angle;
   int16_t type;
   int16_t options;
};

#define MAPTHING_ORIG_SIZEOF 10

#define DEN_SPECTRE    58    // Only in vanilla
#define DEN_HANGINGLEG 62    // Closest vanilla actor type to the bloody chain
#define DEN_CHAIN      64    // Arch-vile in vanilla, bloody chain in PSX
#define DEN_DEMON      3002  // Used both by vanilla and PSX
#define DEN_CACODEMON  3005

#define DEN_EE_NMSPECTRE 889
#define DEN_EE_SPECTRE0  892
#define DEN_EE_SPECTRE1  893
#define DEN_EE_SPECTRE3  890
#define DEN_EE_SPECCACO  894
#define DEN_EE_CHAIN     891

void D_TranslateLevelToVanilla(WadDirectory &dir, const qstring &outName);

#endif

// EOF
