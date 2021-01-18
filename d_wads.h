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
//    Wad file management for finding and loading WADs from the PSX Doom
//    disk's directory struture.
//
//-----------------------------------------------------------------------------

#ifndef D_WADS_H__
#define D_WADS_H__

#include "w_wad.h"

class  qstring;
struct ziparchive_t;

void D_LoadInputFiles(const qstring &inpath);
void D_AddMapsToZip(ziparchive_t *zip, const qstring &inpath);
bool D_IsFinalDoom();

extern WadDirectory psxIWAD;

#endif

// EOF

