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

#include "z_zone.h"

#include "i_opndir.h"
#include "i_system.h"
#include "m_collection.h"
#include "m_qstr.h"
#include "w_wad.h"

DIR          *inputDir;  // input directory
WadDirectory  psxIWAD;   // IWAD directory

#ifdef OPEN_LEVEL_PWADS
PODCollection<WadDirectory *> levelWads; // PWAD directories
#endif

//
// D_verifyInputDirectory
//
// Check that the input directory contains the expected subdirectories
//
static void D_verifyInputDirectory(const qstring &inpath)
{
   if(!(inputDir = opendir(inpath.constPtr())))
   {
      I_Error("D_verifyInputDirectory: cannot open input directory '%s'\n", 
              inpath.constPtr());
   }

   dirent *entry;

   int score = 0;
   while((entry = readdir(inputDir)))
   {
      if(!strcasecmp(entry->d_name, "ABIN"))
         ++score;
      if(!strcasecmp(entry->d_name, "CDAUDIO"))
         ++score;
      if(!strcasecmp(entry->d_name, "MAPDIR0"))
         ++score;
   }
   rewinddir(inputDir);

   if(score < 3)
      I_Error("D_verifyInputDirectory: not PSX Doom root directory?\n");
}

//=============================================================================
//
// Open Wad files
//

//
// D_openPSXIWAD
//
// Open the PSX IWAD file into its own private wad directory.
//
static void D_openPSXIWAD(const qstring &inpath)
{
   qstring filepath = inpath;
   filepath.pathConcatenate("ABIN/PSXDOOM.WAD");

   if(!psxIWAD.addNewPrivateFile(filepath.constPtr()))
      I_Error("D_openPSXIWAD: cannot open '%s'\n", filepath.constPtr());
}

#ifdef OPEN_LEVEL_PWADS
//
// D_openLevelPWADs
//
// Open PSX's individual map PWAD files from the MAPDIR%d subdirectories
// that are found to exist.
//
static void D_openLevelPWADs(const qstring &inpath)
{
   dirent *file;

   while((file = readdir(inputDir)))
   {
      if(!strncasecmp(file->d_name, "MAPDIR", 6))
      {
         DIR    *subdir;
         dirent *wadfile;
         qstring subdirpath = inpath;
         subdirpath.pathConcatenate(file->d_name);

         if(!(subdir = opendir(subdirpath.constPtr())))
            continue;

         // look for .wad files in the subdirectory
         while((wadfile = readdir(subdir)))
         {
            qstring filename = subdirpath;
            filename.pathConcatenate(wadfile->d_name);

            if(filename.findSubStrNoCase(".WAD"))
            {
               WadDirectory *leveldir = new WadDirectory;
               if(leveldir->addNewPrivateFile(filename.constPtr()))
                  levelWads.add(leveldir);
               else
                  delete leveldir;
            }
         }

         closedir(subdir);
      }
   }
}
#endif

//=============================================================================
//
// Interface
//

void D_LoadInputFiles(const qstring &inpath)
{
   // open and verify the input directory path
   D_verifyInputDirectory(inpath);

   // Load IWAD
   D_openPSXIWAD(inpath);

#ifdef OPEN_LEVEL_PWADS
   // Load Level PWADs
   D_openLevelPWADs(inpath);
#endif
}

// EOF

