// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Original author: "natt"
// Modifications for psxwadgen Copyright(C) 2013 James Haley
//
// For this module only:
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// Redistributions of source code must retain the above copyright notice, this
// list of conditions and the following disclaimer. Redistributions in binary
// form must reproduce the above copyright notice, this list of conditions and
// the following disclaimer in the documentation and/or other materials 
// provided with the distribution. The name of the author may not be used to 
// endorse or promote products derived from this software without specific 
// prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
// IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
// OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
// IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
// INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
// BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
// USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
// THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//    Code to extract sound effects from the LCD files.
//
//-----------------------------------------------------------------------------

#include "z_zone.h"

#include "doomtype.h"
#include "i_opndir.h"
#include "i_system.h"
#include "m_buffer.h"
#include "m_misc.h"
#include "m_qstr.h"
#include "m_strcasestr.h"
#include "s_sounds.h"
#include "v_loading.h"
#include "zip_write.h"

typedef int8_t   s8;
typedef uint8_t  u8;
typedef int16_t  s16;
typedef uint16_t u16;
typedef int32_t  s32;
typedef uint32_t u32;

// Directories from which to load all MAP??.LCD
static const char *sndDirs[] =
{
   "SNDMAPS1",
   "SNDMAPS2",
   "SNDMAPS3"
};

struct sfx_t
{
   u8            *data;  // the actual contents of entry
   unsigned int   len;   // length of data
   u8            *pcm;   // rendered PCM data
   unsigned int   nsamp; // length of PCM data in samples
   size_t         total; // total size of PCM data
};

static sfx_t sfx[256];

//
// S_renderPCM
//
// Decompress PlayStation ADPCM to PCM.
//
static void S_renderPCM()
{
   // constants lifted from spxjin.  i don't pretend to understand
   // all of the details of this particular DPCM encoding, but the
   // basic concept isnt that hard...
   const static int f[5][2] = 
   { 
      {    0,  0  },
      {   60,  0  },
      {  115, -52 },
      {   98, -55 },
      {  122, -60 }
   };

   s16 s_1, s_2;

   for(int i = 0; i < 256; i++)
   {
      if(!sfx[i].data || !sfx[i].len)
         continue;

      // each block of 16 bytes gives 28 samples of output data.
      sfx[i].nsamp = sfx[i].len / 16 * 28;

      // we'll make single channel u8 pcm data
      sfx[i].total = 8 + 32 + sfx[i].nsamp;
      sfx[i].pcm = ecalloc(u8 *, 1, sfx[i].total);

      // haleyjd: write in DMX header
      sfx[i].pcm[0] = 0x03; // format 0x03
      sfx[i].pcm[2] = 0x11; // sample rate 11025 Hz
      sfx[i].pcm[3] = 0x2B;
      
      // number of samples + pad bytes
      uint32_t paddedlen = sfx[i].nsamp + 32;
      sfx[i].pcm[4] = (u8)((paddedlen >>  0) & 0xff);
      sfx[i].pcm[5] = (u8)((paddedlen >>  8) & 0xff);
      sfx[i].pcm[6] = (u8)((paddedlen >> 16) & 0xff);
      sfx[i].pcm[7] = (u8)((paddedlen >> 24) & 0xff);

      // set predictor values
      s_1 = 0;
      s_2 = 0;

      u8 *pos;
      u8 *outpos = sfx[i].pcm + 24;

      for(pos = sfx[i].data; pos < sfx[i].data + sfx[i].len; pos += 16)
      {
         // first byte of data: two nibbles, predict weight and shift factor
         int predict_weight = pos[0] >> 4;
         int shift_factor   = pos[0] & 15;

         // second byte of data is flags, we use those elsewhere
         // 4: set loop start point
         // 1: stop (2 = halt, not 2 = loop)

         // remaining bytes are dpcm values for 28 samples in nibbles

         for(int j = 2; j < 16; j++)
         {
            s32 samp = pos[j] & 15; // low nibble

            for(int k = 0; k < 2; k++)
            {
               samp <<= 12; // convert to 16.16

               if(samp & 0x8000) 
                  samp |= 0xffff0000; // sign extend

               samp >>= shift_factor; // apply shift

               samp += s_1 * f[predict_weight][0] >> 6;
               samp += s_2 * f[predict_weight][1] >> 6; // add weighted previous two samples

               // clip
               if(samp > 32767)
                  samp = 32767;
               else if(samp < -32768)
                  samp = -32768;

               s_2 = s_1;
               s_1 = (s16)samp; // shift time fowards 1 sample
               
               // haleyjd: transform to unsigned 8-bit vanilla format
               *outpos++ = (u8)((samp + 32768) >> 8);

               // second iteration: high nibble
               samp = pos[j] >> 4;
            }
         }
      }

      // go back in and populate the DMX padding bytes on either side of the samples
      u8 fs = sfx[i].pcm[24];
      u8 ls = sfx[i].pcm[24+sfx[i].nsamp-1];

      memset(sfx[i].pcm + 8, fs, 16);
      memset(sfx[i].pcm + 24 + sfx[i].nsamp, ls, 16);
   }
}

//
// S_addSFX
//
// Add a sound effect to the list by its id number.
//
static void S_addSFX(u8 *data, unsigned int len, int num)
{
   // if no data, add it
   if(sfx[num].data)
      return;

   sfx[num].data = emalloc(u8 *, len);
   sfx[num].len  = len;
   memcpy(sfx[num].data, data, len);
}

static const u8 zeroblock[16] = {0};

//
// S_parseLCDFile
//
// Parse individual ADPCM-compressed samples from an LCD file.
//
static void S_parseLCDFile(InBuffer &f)
{
   // the file begins with a single u16le which is a number of entries,
   // and then that many u16le of entry ids
   int i;
   u16  nument;
   u16 *entries;

   V_LoadingIncrease();

   f.readUint16(nument);

   u8 block[16];

   if(nument >= 256)
      I_Error("S_parseLCDFile: %i entries (> 256)\n", nument);

   entries = ecalloc(u16 *, sizeof(u16), nument);

   for(i = 0; i < nument; i++)
      f.readUint16(entries[i]);

   for(i = 0; i < nument; i++)
   {
      if(entries[i] >= 256)
         I_Error("S_parseLCDFile: entry %i id %i > 256\n", i, entries[i]);
   }

   // align read pointer to 16 byte boundaries
   i = 2 * (nument + 1);
   if(i & 15)
   {
      i = (i & ~15) + 16 - i;
      f.seek(i, SEEK_CUR);
   }

   u8 *output = NULL;
   unsigned int outputalloc = 0;
   unsigned int outputsize  = 0;

   bool insound  = false;
   int  entrynum = -1;

   while(f.read(block, 16) == 16)
   {
      if(!insound)
      { 
         // assume that any all-zero blocks are padding.  start sound on first non-zero block
         if(memcmp(block, zeroblock, 16) == 0)
            continue;

         // otherwise, start new sound
         ++entrynum;

         outputalloc = 300000;
         output      = ecalloc(u8 *, 1, outputalloc);
         insound     = true;
         outputsize  = 0;
      }
    
      if(outputsize + 16 >= outputalloc)
      {
         outputalloc += 256;
         output = erealloc(u8 *, output, outputalloc);
      }

      memcpy(output + outputsize, block, 16);
      outputsize += 16;

      if(block[1] & 1) // end of sound flag
      {
         insound = false;

         if(entrynum >= nument)
         {
            static int freeid = 128;
            S_addSFX(output, outputsize, freeid++);
         }
         else
            S_addSFX(output, outputsize, entries[entrynum]);

         efree(output);
         outputalloc = 0;
      }
   }

   efree(entries);
}

//
// S_openMainLCD
//
// Open and parse the main LCD file.
//
static void S_openMainLCD(const qstring &inpath)
{
   qstring  fullpath = inpath;
   qstring  musicdir;
   qstring  doomsfx;
   InBuffer infile;

   // look for MUSIC dir in main input directory
   if(!M_FindCanonicalForm(fullpath, "MUSIC", musicdir))
      I_Error("S_openMainLCD: cannot find MUSIC directory\n");   
   fullpath.pathConcatenate(musicdir.constPtr());

   // look for DOOMSFX.LCD file
   if(!M_FindCanonicalForm(fullpath, "DOOMSFX.LCD", doomsfx))
      I_Error("S_openMainLCD: cannot find DOOMSFX.LCD file\n");
   fullpath.pathConcatenate(doomsfx.constPtr());

   if(!infile.openFile(fullpath.constPtr(), InBuffer::LENDIAN))
      I_Error("S_openMainLCD: cannot open %s\n", fullpath.constPtr());

   S_parseLCDFile(infile);

   infile.Close();
}

//
// S_openMapDirLCDs
//
// Open all MAP??.LCD files in a single directory.
//
static void S_openMapDirLCDs(const qstring &dirpath)
{
   DIR    *dir;
   dirent *ent;
   
   if(!(dir = opendir(dirpath.constPtr())))
      return; // feh.

   while((ent = readdir(dir)))
   {
      if(strncasecmp(ent->d_name, "MAP", 3) || // must start with MAP
         !M_StrCaseStr(ent->d_name, ".LCD"))   // must be an LCD file
         continue;

      qstring fn = dirpath;
      fn.pathConcatenate(ent->d_name);

      InBuffer infile;
      
      if(!infile.openFile(fn.constPtr(), InBuffer::LENDIAN))
         continue;

      S_parseLCDFile(infile);

      infile.Close();
   }

   closedir(dir);
}

//
// S_openAllMapLCDs
//
// Sweep each SNDMAPS directory for LCD files
//
static void S_openAllMapLCDs(const qstring &inpath)
{
   for(size_t i = 0; i < earrlen(sndDirs); i++)
   {
      qstring dirpath = inpath;
      qstring snddir;

      if(!M_FindCanonicalForm(dirpath, sndDirs[i], snddir))
         continue;
      dirpath.pathConcatenate(snddir.constPtr());

      S_openMapDirLCDs(dirpath);
   }
}

//
// S_loadSounds
//
// Load all sounds from LCD files and then convert them to PCM data.
//
static void S_loadSounds(const qstring &inpath)
{
   // Load LCDs
   printf("S_LoadSounds: Loading LCD files:");
   V_SetLoading(61, true);
   S_openMainLCD(inpath);
   S_openAllMapLCDs(inpath);

   // Render PCM
   printf("S_RenderPCM: Decoding ADPCM data\n");
   S_renderPCM();
}

//=============================================================================
//
// Interface
//

//
// S_ProcessSoundsForZip
//
// Read in all the sound data and then output file entries to the zip archive.
//
void S_ProcessSoundsForZip(const qstring &inpath, ziparchive_t *zip)
{
   // load sounds from all of the LCD files and decode the ADPCM data
   S_loadSounds(inpath);

   Zip_AddFile(zip, "sounds/", NULL, 0, ZIP_DIRECTORY, false);

   for(int i = 0; i < NUMPSXSFX; i++)
   {
      auto &sfxinfo = psxsfxinfo[i];
      auto &snd     = sfx[sfxinfo.sfxID];
      qstring name;

      if(!snd.pcm || !snd.total)
         continue;

      name << "sounds/" << sfxinfo.name;

      Zip_AddFile(zip, name.constPtr(), snd.pcm, snd.total, ZIP_FILE_BINARY, true);
   }
}

//=============================================================================
//
// Unit Tests
//

#ifndef NO_UNIT_TESTS

//
// S_UnitTest1
//
// Sound Unit Test 1 - write all loaded LCD patches
//
void S_UnitTest1(const qstring &inpath)
{
   edefstructvar(ziparchive_t, zip);

   S_loadSounds(inpath);

   Zip_Create(&zip, "soundtest.pke");

   Zip_AddFile(&zip, "sounds/", NULL, 0, ZIP_DIRECTORY, false);

   for(int i = 0; i < 256; i++)
   {
      qstring name;

      if(!sfx[i].pcm || !sfx[i].total)
         continue;

      name << "sounds/psxsnd" << i << ".lmp";

      Zip_AddFile(&zip, name.constPtr(), sfx[i].pcm, sfx[i].total, ZIP_FILE_BINARY, true);
   }

   Zip_Write(&zip);
}

//
// S_UnitTest2
//
// Sound Unit Test 2 - write sfx based on psxsfxinfo table.
//
void S_UnitTest2(const qstring &inpath)
{
   edefstructvar(ziparchive_t, zip);

   S_loadSounds(inpath);

   Zip_Create(&zip, "soundtest2.pke");

   Zip_AddFile(&zip, "sounds/", NULL, 0, ZIP_DIRECTORY, false);

   for(int i = 0; i < NUMPSXSFX; i++)
   {
      auto &sfxinfo = psxsfxinfo[i];
      auto &snd     = sfx[sfxinfo.sfxID];
      qstring name;

      if(!snd.pcm || !snd.total)
         continue;

      name << "sounds/" << sfxinfo.name << ".lmp";

      Zip_AddFile(&zip, name.constPtr(), snd.pcm, snd.total, ZIP_FILE_BINARY, true);
   }

   Zip_Write(&zip);
}

#endif

// EOF

