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
#include "m_misc.h"
#include "m_qstr.h"
#include "v_loading.h"
#include "w_wad.h"
#include "w_iterator.h"
#include "z_auto.h"
#include "zip_write.h"

DIR          *inputDir;  // input directory
WadDirectory  psxIWAD;   // IWAD directory

#ifdef OPEN_LEVEL_PWADS
PODCollection<WadDirectory *> levelWads; // PWAD directories
#endif

// Remember the canonical form of the ABIN directory name
static qstring abin; 

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
      {
         // remember canonical form of ABIN path on disk
         abin = entry->d_name;
         ++score;
      }
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
   qstring psxdoomwad;
   
   // look in PSXDOOM/ABIN directory for a PSXDOOM.WAD file
   filepath.pathConcatenate(abin.constPtr());
   if(!M_FindCanonicalForm(filepath, "PSXDOOM.WAD", psxdoomwad))
      I_Error("D_OpenPSXIWAD: PSXDOOM.WAD not found\n");

   filepath.pathConcatenate(psxdoomwad.constPtr());

   if(!psxIWAD.addNewFile(filepath.constPtr()))
      I_Error("D_openPSXIWAD: cannot open '%s'\n", filepath.constPtr());

   printf(" added %s\n", filepath.constPtr());
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

#define PUTLONG(b, l) \
   *(b + 0) = (byte)((l >>  0) & 0xff); \
   *(b + 1) = (byte)((l >>  8) & 0xff); \
   *(b + 2) = (byte)((l >> 16) & 0xff); \
   *(b + 3) = (byte)((l >> 24) & 0xff); \
   b += 4

//
// D_AddOneMapToZip
//
// What a goddamn pain in the ass.
//
static void D_addOneMapToZip(ziparchive_t *zip, const char *name, const qstring &filename)
{
   WadDirectory dir;

   if(!dir.addNewFile(filename.constPtr()))
      I_Error("D_addOneMapToZip: cannot open file %s\n", filename.constPtr());

   WadNamespaceIterator wni(dir, lumpinfo_t::ns_global);

   wadinfo_t header  = { {'P','W','A','D'}, wni.getNumLumps(), 12 };
   size_t sizeNeeded = 12 + 16 * wni.getNumLumps();
   size_t filepos    = sizeNeeded;

   for(wni.begin(); wni.current(); wni.next())
      sizeNeeded += wni.current()->size;

   if(!sizeNeeded)
      I_Error("D_addOneMapToZip: zero-size wad file %s\n", filename.constPtr());

   auto buffer = ecalloc(byte *, 1, sizeNeeded);

   // write in header
   memcpy(buffer, header.identification, 4);
   byte *inptr = buffer + 4;
   PUTLONG(inptr, header.numlumps);
   PUTLONG(inptr, header.infotableofs);
   
   // write in directory
   for(wni.begin(); wni.current(); wni.next())
   {
      lumpinfo_t *lump = wni.current();
      PUTLONG(inptr, filepos);
      PUTLONG(inptr, lump->size);
      memcpy(inptr, lump->name, 8);
      inptr += 8;
      filepos += lump->size;
   }

   // write in lumps
   for(wni.begin(); wni.current(); wni.next())
   {
      lumpinfo_t *lump = wni.current();
      if(lump->size > 0)
      {
         ZAutoBuffer buf;
         dir.cacheLumpAuto(lump->selfindex, buf);
         memcpy(inptr, buf.get(), lump->size);
         inptr += lump->size;
      }
   }

   Zip_AddFile(zip, name, buffer, (uint32_t)sizeNeeded, ZIP_FILE_BINARY, true);
}

//=============================================================================
//
// Interface
//

//
// D_LoadInputFiles
//
// Call at startup to verify the input directory path and open the IWAD (and
// possibly PWADs when we get a psxdoom.wad target implemented).
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

//
// D_AddMapsToZip
//
// Add the files in the map directories to a zip output file. The actual files
// won't be read until the zip file is being written out.
//
void D_AddMapsToZip(ziparchive_t *zip, const qstring &inpath)
{
   dirent *file;

   printf("D_AddMaps: adding map wadfiles.\n");

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
               D_addOneMapToZip(zip, wadfile->d_name, filename);
         }

         closedir(subdir);
      }
   }
   
   rewinddir(inputDir);
}

// EOF

