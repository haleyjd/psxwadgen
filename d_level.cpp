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
// PURPOSE: Level translator - allows output of vanilla Doom format levels
//

#include "z_zone.h"
#include "z_auto.h"
#include "doomtype.h"
#include "i_system.h"
#include "m_argv.h"
#include "m_binary.h"
#include "m_fixed.h"
#include "m_misc.h"
#include "m_qstr.h"
#include "w_wad.h"
#include "w_iterator.h"
#include "d_level.h"

#define DOOM_VERTEX_LEN 4
#define PSX_VERTEX_LEN  8

//
// Get output size for VERTEXES lump
//
static size_t D_VERTEXESLen(size_t inSize)
{
   return (inSize / PSX_VERTEX_LEN) * DOOM_VERTEX_LEN;
}

//
// Translate VERTEXES lump
//
static void D_TranslateVERTEXES(byte *inData, size_t inSize, byte *&outData)
{
   // input is two 32-bit fixed point coordinates per vertex
   // output is two shorts per vertex
   while(inSize > 0)
   {
      fixed_t x = GetBinaryDWord(&inData);
      fixed_t y = GetBinaryDWord(&inData);

      int16_t outX = int16_t(x / FRACUNIT);
      int16_t outY = int16_t(y / FRACUNIT);

      // round if fractional portion is >= 0.5
      if(abs(x & (FRACUNIT - 1)) >= FRACUNIT / 2)
      {
         if(x < 0)
            --outX;
         else if(x >= 0)
            ++outX;
      }
      if(abs(y & (FRACUNIT - 1)) >= FRACUNIT / 2)
      {
         if(y < 0)
            --outY;
         else if(y >= 0)
            ++outY;
      }
      
      PutBinaryWord(outData, outX);
      PutBinaryWord(outData, outY);

      inSize -= PSX_VERTEX_LEN;
   }
}

#define DOOM_SECTOR_SIZE 26
#define PSX_SECTOR_SIZE  28

//
// Get output size for SECTORS lump
//
static size_t D_SECTORSLen(size_t inSize)
{
   return (inSize / PSX_SECTOR_SIZE) * DOOM_SECTOR_SIZE;
}

//
// Translate SECTORS lump
//
static void D_TranslateSECTORS(byte *inData, size_t inSize, byte *&outData)
{
   while(inSize > 0)
   {
      memcpy(outData, inData, DOOM_SECTOR_SIZE); // Doom sector is a strict subset of the PSX data
      outData += DOOM_SECTOR_SIZE; // wrote a Doom sector
      inData  += PSX_SECTOR_SIZE;  // consumed a PSX sector
      inSize  -= PSX_SECTOR_SIZE;
   }
}

enum thingtlflags_e
{
   TTLF_FIXTHINGS    = 1,
   TTLF_USE_EE_DNUMS = 2  // Target Eternity's custom thing types
};

//
// Translate THINGS lump - certain types of transforms may be optionally applied
// to change thing types that are specific to the PlayStation port.
//
static void D_TranslateTHINGS(byte *inData, size_t inSize, byte *&outData, uint tlflags)
{
   size_t numthings = inSize / MAPTHING_ORIG_SIZEOF;

   for(size_t i = 0; i < numthings; i++)
   {
      mapthing_t mt;
      mt.x       = GetBinaryWord(&inData);
      mt.y       = GetBinaryWord(&inData);
      mt.angle   = GetBinaryWord(&inData);
      mt.type    = GetBinaryWord(&inData);
      mt.options = GetBinaryWord(&inData);

      // fix up thing types?
      if(tlflags & TTLF_FIXTHINGS)
      {
         psxblendmode_e spectreType = static_cast<psxblendmode_e>((mt.options & MTF_PSX_BLENDMASK) >> MTF_BLENDSHIFT);
         switch(mt.type)
         {
         case DEN_DEMON:
            if(mt.options & MTF_PSX_USEBLEND)
            {
               if(tlflags & TTLF_USE_EE_DNUMS)
               {
                  switch(spectreType)
                  {
                  case PBM_TL50:
                     mt.type = DEN_EE_SPECTRE0;
                     break;
                  case PBM_TLADD100:
                     mt.type = DEN_EE_SPECTRE1;
                     break;
                  case PBM_NIGHTMARE:
                     mt.type = DEN_EE_NMSPECTRE;
                     break;
                  case PBM_TLADD25:
                     mt.type = DEN_EE_SPECTRE3;
                     break;
                  }
               }
               else
                  mt.type = DEN_SPECTRE;
            }
            break;
         case DEN_CACODEMON:
            if(mt.options & MTF_PSX_USEBLEND && tlflags & TTLF_USE_EE_DNUMS)
            {
               if(spectreType == PBM_TL50)
                  mt.type = DEN_EE_SPECCACO;
               else
                  printf("Warning: unknown Cacodemon type at (%d, %d); spectre type = %d\n", mt.x, mt.y, spectreType);
            }
            break;
         case DEN_CHAIN:
            if(tlflags & TTLF_USE_EE_DNUMS)
               mt.type = DEN_EE_CHAIN;
            else
               mt.type = DEN_HANGINGLEG;
            break;
         default:
            if(mt.options & MTF_PSX_USEBLEND)
               printf("Warning: unknown spectral thing type %d at (%d, %d); spectre type = %d\n", mt.type, mt.x, mt.y, spectreType);
            break;
         }

         // remove the special flags since they conflict with ports like Boom and MBF
         mt.options &= ~(MTF_PSX_USEBLEND|MTF_PSX_BLENDMASK);
      }

      PutBinaryWord(outData, mt.x);
      PutBinaryWord(outData, mt.y);
      PutBinaryWord(outData, mt.angle);
      PutBinaryWord(outData, mt.type);
      PutBinaryWord(outData, mt.options);
   }
}

//
// Ouput a vanilla-format level wad for PSX Doom level input.
// Thing types, line specials, and line flags are not currently accounted for.
// Textures are not translated - TODO: create a separate texture wad to use.
//
void D_TranslateLevelToVanilla(WadDirectory &dir, const qstring &outName)
{
   uint tlflags = 0;
   if(M_CheckParm("-fixthings") != 0)
      tlflags |= TTLF_FIXTHINGS;
   if(M_CheckParm("-ee") != 0)
      tlflags |= TTLF_USE_EE_DNUMS;

   WadNamespaceIterator wni(dir, lumpinfo_t::ns_global);

   size_t sizeNeeded = 0;
   int    numLumps   = 0;

   // add in total of lump sizes
   for(wni.begin(); wni.current(); wni.next())
   {
      lumpinfo_t *curLump = wni.current();

      if(!strcasecmp(curLump->name, "SECTORS"))
         sizeNeeded += D_SECTORSLen(curLump->size);
      else if(!strcasecmp(curLump->name, "VERTEXES"))
         sizeNeeded += D_VERTEXESLen(curLump->size);
      else if(!strcasecmp(curLump->name, "LEAFS"))
         continue; // don't include LEAFS
      else
         sizeNeeded += curLump->size;

      ++numLumps;
   }

   // setup header and calculate directory size
   wadinfo_t header  = { {'P','W','A','D'}, numLumps, 12 };
   size_t    dirSize = 12 + 16 * numLumps;
   size_t    filepos = dirSize;

   // add in directory size
   sizeNeeded += dirSize;

   if(!sizeNeeded)
      I_Error("D_TranslateLevelToVanilla: zero-size wad file\n");

   auto buffer = ecalloc(byte *, 1, sizeNeeded);

   // write in header
   memcpy(buffer, header.identification, 4);
   byte *inptr = buffer + 4;
   PutBinaryDWord(inptr, header.numlumps);
   PutBinaryDWord(inptr, header.infotableofs);

   // write in directory
   for(wni.begin(); wni.current(); wni.next())
   {
      lumpinfo_t *lump = wni.current();
      if(!strcasecmp(lump->name, "LEAFS"))
         continue; // don't include LEAFS

      PutBinaryDWord(inptr, int32_t(filepos));

      size_t sizeToUse = lump->size;
      if(!strcasecmp(lump->name, "SECTORS"))
         sizeToUse = D_SECTORSLen(lump->size);
      else if(!strcasecmp(lump->name, "VERTEXES"))
         sizeToUse = D_VERTEXESLen(lump->size);

      PutBinaryDWord(inptr, int32_t(sizeToUse));
      memcpy(inptr, lump->name, 8);
      inptr += 8;
      filepos += sizeToUse;
   }

   // write in lumps
   for(wni.begin(); wni.current(); wni.next())
   {
      lumpinfo_t *lump = wni.current();
      if(lump->size > 0)
      {
         ZAutoBuffer buf;
         dir.cacheLumpAuto(lump->selfindex, buf);

         if(!strcasecmp(lump->name, "SECTORS"))
            D_TranslateSECTORS(buf.getAs<byte *>(), lump->size, inptr);
         else if(!strcasecmp(lump->name, "VERTEXES"))
            D_TranslateVERTEXES(buf.getAs<byte *>(), lump->size, inptr);
         else if(!strcasecmp(lump->name, "THINGS") && tlflags != 0) // only translate things if so instructed
            D_TranslateTHINGS(buf.getAs<byte *>(), lump->size, inptr, tlflags);
         else if(!strcasecmp(lump->name, "LEAFS"))
            continue; // don't include LEAFS
         else
         {
            memcpy(inptr, buf.get(), lump->size);
            inptr += lump->size;
         }
      }
   }

   // output wad file
   M_WriteFile(outName.constPtr(), buffer, sizeNeeded);
   efree(buffer);
}
// EOF
