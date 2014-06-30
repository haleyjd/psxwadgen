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

#include "i_system.h"
#include "m_misc.h"
#include "m_qstr.h"
#include "main.h"
#include "zip_write.h"

//
// D_addResourceScriptToZip
//
// Add a file from the resource directory as a script lump.
//
static void D_addResourceScriptToZip(ziparchive_t *zip, const char *filename)
{
   qstring srcpath;

   srcpath = filename;
   D_MakeResourceFilePath(srcpath);

   char *text;
   if(!(text = M_LoadStringFromFile(srcpath.constPtr())))
      I_Error("D_addResourceScript: unable to load resource %s\n", srcpath.constPtr());

   Zip_AddFile(zip, filename, reinterpret_cast<byte *>(text), 
               static_cast<uint32_t>(strlen(text)), ZIP_FILE_TEXT, true);
}

//
// D_addBinaryFileToZip
//
// Add a binary file from the /res folder into the zip.
//
static void D_addBinaryFileToZip(ziparchive_t *zip, const char *filename, 
                                 const char *lumpname)
{
   qstring srcpath;

   srcpath = filename;
   D_MakeResourceFilePath(srcpath);

   Zip_AddFile(zip, lumpname, srcpath.constPtr(), false);
}

//
// D_addSwitchesAndAnimToZip
//
// Add the SWITCHES and ANIMATED lumps.
//
static void D_addSwitchesAndAnimToZip(ziparchive_t *zip)
{
}

//
// D_ProcessScriptsForZip
//
// Add script lumps to the zip archive.
//
void D_ProcessScriptsForZip(ziparchive_t *zip)
{
   printf("D_ProcessScripts: adding script files.\n");

   // EDFROOT
   D_addResourceScriptToZip(zip, "EDFROOT.edf");

   // EMAPINFO, for Eternity LevelInfo
   D_addResourceScriptToZip(zip, "EMAPINFO.txt");

   // gameversion.txt, for Eternity gamemode determination
   D_addResourceScriptToZip(zip, "gameversion.txt");

   // add ANIMATED and SWITCHES lumps
   D_addBinaryFileToZip(zip, "ANIMATED.LMP", "ANIMATED.lmp");
   D_addBinaryFileToZip(zip, "SWITCHES.LMP", "SWITCHES.lmp");
}

// EOF

