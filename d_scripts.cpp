// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2014 James Haley
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
//    Writing of script lumps.
//
//-----------------------------------------------------------------------------

#include "z_zone.h"

#include "zip_write.h"

// Game version lump contents
// TODO: change to "doom2 psx" as soon as supported
static const char *gameversion = "doom2 commercial";

//
// D_ProcessScriptsForZip
//
// Add script lumps to the zip archive.
//
void D_ProcessScriptsForZip(ziparchive_t *zip)
{
   // Add gameversion.txt
   Zip_AddFile(zip, "gameversion.txt", reinterpret_cast<const byte *>(gameversion),
               static_cast<uint32_t>(strlen(gameversion)), ZIP_FILE_TEXT, false);
}

// EOF

