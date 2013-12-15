// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2013 James Haley, Samuel Villarreal
//
// Some code derived from SLADE:
// SLADE - It's a Doom Editor
// Copyright (C) 2008-2012 Simon Judd
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

#include "z_zone.h"

#include "m_collection.h"
#include "m_fixed.h"
#include "m_misc.h"
#include "m_qstr.h"
#include "m_swap.h"
#include "r_patch.h"
#include "v_psx.h"
#include "v_loading.h"
#include "w_wad.h"
#include "w_iterator.h"
#include "z_auto.h"
#include "zip_write.h"

// PSX weapon sprites have a special issue in that the game is coded to take
// the offset in the sprite and use it relative to a point in the center of
// the screen and at the top of the status bar. We will have to adjust such
// sprites' offsets in order to have them appear correct in normal ports.
static const char *weaponSprites[] =
{
   "PUNG",
   "PISG", "PISF",
   "SHTG", "SHTF",
   "SHT2",
   "CHGG", "CHGF",
   "MISG", "MISF",
   "SAWG",
   "PLSG", "PLSF",
   "BFGG", "BFGF"
};

#define WEAPON_ORIGIN_X -160
#define WEAPON_ORIGIN_Y -168

struct psxpic_header_t
{
   int16_t left;
   int16_t top;
   int16_t width;
   int16_t height;
};

#define PSXPIC_HEADER_SIZE (4*sizeof(int16_t))

#define SAFEUINT16(dest, s) dest = SwapShort(*s | (*(s+1) << 8)); read += 2;

//
// VPSXImage::readImage
//
// Read the image from a lump.
//
void VPSXImage::readImage(const void *data)
{
   const uint8_t *read = static_cast<const uint8_t *>(data);

   SAFEUINT16(left,   read);
   SAFEUINT16(top,    read);
   SAFEUINT16(width,  read);
   SAFEUINT16(height, read);

   pixels = ecalloc(uint8_t *, width, height);
   mask   = ecalloc(uint8_t *, width, height);

   memcpy(pixels, read, width * height);
   memset(mask,    255, width * height);

   for(size_t i = 0; i < (size_t)(width*height); i++)
   {
      if(pixels[i] == 0)
         mask[i] = 0;
   }
}

//
// VPSXImage::adjustOffsets
//
// Fixes offsets for weapon sprites.
//
void VPSXImage::adjustOffsets(const char *name)
{
   for(size_t i = 0; i < earrlen(weaponSprites); i++)
   {
      if(!strncasecmp(name, weaponSprites[i], strlen(weaponSprites[i])))
      {
         left += WEAPON_ORIGIN_X;
         top  += WEAPON_ORIGIN_Y;
      }
   }
}

//
// Constructor
// Taking a lump number in the indicated directory to load.
//
VPSXImage::VPSXImage(WadDirectory &dir, int lumpnum)
{
   ZAutoBuffer buf;
   dir.cacheLumpAuto(lumpnum, buf);
   readImage(buf.get());
   adjustOffsets(dir.getLumpInfo()[lumpnum]->name);
}

//
// Constructor.
// Taking a lump name in the indicated directory to load.
//
VPSXImage::VPSXImage(WadDirectory &dir, const char *lumpname)
{
   ZAutoBuffer buf;
   dir.cacheLumpAuto(lumpname, buf);
   readImage(buf.get());
   adjustOffsets(lumpname);
}

class VPatchPost
{
public:
   uint8_t row_off;
   PODCollection<uint8_t> pixels;
};

class VPatchColumn
{
public:
   Collection<VPatchPost> posts;
};

#define PUTBYTE(r, v) *r = (uint8_t)(v); ++r

#define PUTSHORT(r, v)                          \
  *(r+0) = (byte)(((uint16_t)(v) >> 0) & 0xff); \
  *(r+1) = (byte)(((uint16_t)(v) >> 8) & 0xff); \
  r += 2

#define PUTLONG(r, v)                             \
   *(r+0) = (byte)(((uint32_t)(v) >>  0) & 0xff); \
   *(r+1) = (byte)(((uint32_t)(v) >>  8) & 0xff); \
   *(r+2) = (byte)(((uint32_t)(v) >> 16) & 0xff); \
   *(r+3) = (byte)(((uint32_t)(v) >> 24) & 0xff); \
   r += 4

//
// VPSXImage::toPatch
//
// Return the image converted to a patch_t-format lump.
// Mostly straight from SLADE.
//
void *VPSXImage::toPatch(size_t &size) const
{
   Collection<VPatchColumn> columns;

   // Go through columns
   uint32_t offset = 0;
   for(int c = 0; c < width; c++)
   {
      VPatchColumn col;
      VPatchPost   post;
      post.row_off   = 0;
      bool ispost    = false;
      bool first_254 = true;  // first 254 pixels use absolute offsets

      offset = c;
      uint8_t row_off = 0;
      for(int r = 0; r < height; r++)
      {
         // if we're at offset 254, create a dummy post for tall doom gfx support
         if(row_off == 254)
         {
            // Finish current post if any
            if(ispost)
            {
               col.posts.add(post);
               post.pixels.makeEmpty();
               ispost = false;
            }

            // Begin relative offsets
            first_254 = false;

            // Create dummy post
            post.row_off = 254;
            col.posts.add(post);

            // Clear post
            row_off = 0;
            ispost  = false;
         }

         // If the current pixel is not transparent, add it to the current post
         if(mask[offset] > 0)
         {
            // If we're not currently building a post, begin one and set its offset
            if(!ispost)
            {
               // Set offset
               post.row_off = row_off;

               // Reset offset if we're in relative offsets mode
               if(!first_254)
                  row_off = 0;

               // Start post
               ispost = true;
            }

            // Add the pixel to the post
            post.pixels.add(pixels[offset]);
         }
         else if(ispost)
         {
            // If the current pixel is transparent and we are currently building
            // a post, add the current post to the list and clear it
            col.posts.add(post);
            post.pixels.makeEmpty();
            ispost = false;
         }

         // Go to next row
         offset += width;
         ++row_off;
      }

      // If the column ended with a post, add it
      if(ispost)
         col.posts.add(post);

      // Add the column data
      columns.add(col);

      // Go to next column
      ++offset;
   }

   // Calculate needed memory size to allocate patch buffer
   size = 0;
   size += 4 * sizeof(int16_t);                   // 4 header shorts
   size += columns.getLength() * sizeof(int32_t); // offsets table

   for(size_t c = 0; c < columns.getLength(); c++)
   {
      for(size_t p = 0; p < columns[c].posts.getLength(); p++)
      {
         size_t post_len = 0;

         post_len += 2; // two bytes for post header
         post_len += 1; // dummy byte
         post_len += columns[c].posts[p].pixels.getLength(); // pixels
         post_len += 1; // dummy byte

         size += post_len;
      }

      size += 1; // room for 0xff cap byte
   }

   byte *output = ecalloc(byte *, size, 1);
   byte *rover  = output;

   // write header fields
   PUTSHORT(rover, width);
   PUTSHORT(rover, height);
   PUTSHORT(rover, left);
   PUTSHORT(rover, top);

   // set starting position of column offsets table, and skip over it
   byte *col_offsets = rover;
   rover += columns.getLength() * 4;

   for(size_t c = 0; c < columns.getLength(); c++)
   {
      // write column offset to offset table
      uint32_t offs = (uint32_t)(rover - output);
      PUTLONG(col_offsets, offs);

      // write column posts
      for(size_t p = 0; p < columns[c].posts.getLength(); p++)
      {
         // Write row offset
         PUTBYTE(rover, columns[c].posts[p].row_off);

         // Write number of pixels
         size_t numPixels = columns[c].posts[p].pixels.getLength();
         PUTBYTE(rover, numPixels);

         // Write pad byte
         byte lastval = numPixels ? columns[c].posts[p].pixels[0] : 0;
         PUTBYTE(rover, lastval);

         // Write pixels
         for(size_t a = 0; a < numPixels; a++)
         {
            lastval = columns[c].posts[p].pixels[a];
            PUTBYTE(rover, lastval);
         }

         // Write pad byte
         PUTBYTE(rover, lastval);
      }

      // Write 255 cap byte
      PUTBYTE(rover, 0xff);
   }

   // Done!
   return output;
}

//=============================================================================
//
// Sprites
//

//
// V_ConvertSpritesToZip
//
// Convert PSX Doom's sprites to Doom's patch format and insert them into the
// zip under the sprites/ directory.
//
void V_ConvertSpritesToZip(WadDirectory &dir, ziparchive_t *zip)
{
   WadNamespaceIterator wni(dir, lumpinfo_t::ns_sprites);
   int numlumps = wni.getNumLumps();
   fixed_t dotstep  = numlumps ? 64 * FRACUNIT / numlumps : 0;
   fixed_t dotaccum = 0;

   printf("V_ConvertSprites: converting sprites:");
   V_SetLoading(64, true);

   Zip_AddFile(zip, "sprites/", NULL, 0, ZIP_DIRECTORY, false);

   int count = 0;
   for(wni.begin(); wni.current(); wni.next(), count++)
   {
      lumpinfo_t *lump = wni.current();
      VPSXImage img(dir, lump->selfindex);
      size_t  size = 0;
      void   *data = img.toPatch(size);
      qstring name;

      dotaccum += dotstep;
      while(dotaccum >= FRACUNIT)
      {
         V_LoadingIncrease();
         dotaccum -= FRACUNIT;
      }

      name << "sprites/" << lump->name;

      Zip_AddFile(zip, name.constPtr(), (byte *)data, (uint32_t)size, 
                  ZIP_FILE_BINARY, true);
   }

   if(dotaccum != 0)
      V_LoadingIncrease();
}

//=============================================================================
//
// Textures
//

//
// V_ConvertTexturesToZip
//
// Convert the PSX textures into Doom's patch format and insert them into the
// zip under the textures/ directory.
//
void V_ConvertTexturesToZip(WadDirectory &dir, ziparchive_t *zip)
{
   WadNamespaceIterator wni(dir, lumpinfo_t::ns_textures);
   int numlumps = wni.getNumLumps();
   fixed_t dotstep  = numlumps ? 64 * FRACUNIT / numlumps : 0;
   fixed_t dotaccum = 0;

   printf("V_ConvertTextures: converting textures:");
   V_SetLoading(64, true);

   Zip_AddFile(zip, "textures/", NULL, 0, ZIP_DIRECTORY, false);

   int count = 0;
   for(wni.begin(); wni.current(); wni.next(), count++)
   {
      lumpinfo_t *lump = wni.current();
      VPSXImage img(dir, lump->selfindex);
      size_t  size = 0;
      void   *data = img.toPatch(size);
      qstring name;

      dotaccum += dotstep;
      while(dotaccum >= FRACUNIT)
      {
         V_LoadingIncrease();
         dotaccum -= FRACUNIT;
      }

      name << "textures/" << lump->name;

      Zip_AddFile(zip, name.constPtr(), (byte *)data, (uint32_t)size, 
                  ZIP_FILE_BINARY, true);
   }

   if(dotaccum != 0)
      V_LoadingIncrease();
}

//=============================================================================
//
// Flats
//

//
// V_ConvertFlatsToZip
//
// PSX graphics are already linear so this is pretty simple.
//
void V_ConvertFlatsToZip(WadDirectory &dir, ziparchive_t *zip)
{
   WadNamespaceIterator wni(dir, lumpinfo_t::ns_flats);
   int numlumps = wni.getNumLumps();
   fixed_t dotstep  = numlumps ? 64 * FRACUNIT / numlumps : 0;
   fixed_t dotaccum = 0;

   printf("V_ConvertFlats: converting flats:");
   V_SetLoading(64, true);

   Zip_AddFile(zip, "flats/", NULL, 0, ZIP_DIRECTORY, false);

   int count = 0;
   for(wni.begin(); wni.current(); wni.next(), count++)
   {
      lumpinfo_t *lump = wni.current();
      VPSXImage img(dir, lump->selfindex);

      const uint8_t *data = img.getPixels();
      qstring name;

      dotaccum += dotstep;
      while(dotaccum >= FRACUNIT)
      {
         V_LoadingIncrease();
         dotaccum -= FRACUNIT;
      }

      name << "flats/" << lump->name;

      Zip_AddFile(zip, name.constPtr(), data, img.getWidth()*img.getHeight(),
                  ZIP_FILE_BINARY, true);
   }

   if(dotaccum != 0)
      V_LoadingIncrease();
}

//=============================================================================
//
// PLAYPAL Conversion
//

// playpal data in RGB 24-bit format
static byte   *playpal;
static size_t  numpals;

//
// V_LoadPLAYPAL
//
// Load the PLAYPAL lump and convert it from PSX RGBA 1555 format to 24-bit.
// Note that the only colors which ever set the alpha bit in the PSX PLAYPAL 
// are at index zero, so it is fortunately unnecessary to deal with any other
// colors being treated as transparent.
//
// Special thanks to Samuel Villarreal (Kaiser) for help in understanding the
// format.
//
void V_LoadPLAYPAL(WadDirectory &dir)
{
   ZAutoBuffer buf;
   dir.cacheLumpAuto("PLAYPAL", buf);
   
   auto cptr = buf.getAs<uint16_t *>();
   auto len  = buf.getSize();

   numpals = len / 512;
   playpal = ecalloc(byte *, 768, numpals);
   
   byte *palptr = playpal;
   for(size_t palnum = 0; palnum < numpals; palnum++)
   {
      for(int colnum = 0; colnum < 256; colnum++)
      {
         uint16_t val = SwapUShort(*cptr++);

         palptr[3*colnum+0] = (byte)(((val & 0x001F) << 3) + 7);
         palptr[3*colnum+1] = (byte)(((val & 0x03E0) >> 2) + 7);
         palptr[3*colnum+2] = (byte)(((val & 0x7C00) >> 7) + 7);
      }
      palptr += 768;
   }
}

struct nameforpal_t
{
   size_t      palnum; // palette number in PLAYPAL
   const char *name;   // name of lump
};

static nameforpal_t LumpNameForPal[] =
{
   { PAL_GODMODE,   "PALGOD"   },
   { PAL_FIRESKY,   "PALFIRE"  },
   { PAL_LEGALS,    "PALLEGAL" },
   { PAL_DOOMTITLE, "PALTITLE" },
   { PAL_IDCRED1,   "PALIDCRD" },
   { PAL_WMSCRED1,  "PALWMCRD" },
};

//
// V_ConvertPLAYPALToZip
//
// Place the PLAYPAL lump in the root directory of the zip. Non-standard
// palettes from the lump will also be saved separately for potential
// convenience of use.
//
void V_ConvertPLAYPALToZip(ziparchive_t *zip)
{
   printf("V_ConvertPLAYPAL: Converting palettes.\n");
   Zip_AddFile(zip, "PLAYPAL", playpal, 768*numpals, ZIP_FILE_BINARY, false);

   for(size_t i = 0; i < earrlen(LumpNameForPal); i++)
   {
      auto nameforpal = &LumpNameForPal[i];
      if(numpals > nameforpal->palnum)
      {
         Zip_AddFile(zip, nameforpal->name, playpal + 768 * nameforpal->palnum, 
                     768, ZIP_FILE_BINARY, false);
      }
   }
}

#ifndef NO_UNIT_TESTS
//
// V_ExplodePLAYPAL
//
// This is a debug function that will extract the 20 separate palettes from the
// PSX PLAYPAL into 20 files.
//
void V_ExplodePLAYPAL(WadDirectory &dir)
{
   ZAutoBuffer buf;
   dir.cacheLumpAuto("PLAYPAL", buf);

   auto cptr = buf.getAs<uint16_t *>();

   for(int palnum = 0; palnum < 20; palnum++)
   {
      char palname[15] = "PLAYPAL00.lmp\0";
      uint8_t convPal[768];
      
      for(int colnum = 0; colnum < 256; colnum++)
      {
         uint16_t val = *cptr++;

         convPal[3*colnum+0] = (uint8_t)(((val & 0x001F) << 3) + 7);
         convPal[3*colnum+1] = (uint8_t)(((val & 0x03E0) >> 2) + 7);
         convPal[3*colnum+2] = (uint8_t)(((val & 0x7C00) >> 7) + 7);
      }
      palname[7] = '0' + palnum/10;
      palname[8] = '0' + palnum%10;

      M_WriteFile(palname, convPal, 768);
   }

   // also dump the original lump
   M_WriteFile("PLAYPAL.lmp", buf.get(), buf.getSize());
}
#endif

//=============================================================================
//
// Color Matcher
//
// Code is from SLADE.
//

//
// V_ColoursFromPLAYPAL
//
// Create rgba_t structures for all the entries in a specific playpal.
//
void V_ColoursFromPLAYPAL(size_t palnum, rgba_t outpal[256])
{
   if(palnum >= numpals)
      return;

   byte *palbase = playpal + 768*palnum;
   for(int i = 0; i < 256; i++)
   {
      outpal[i].r = palbase[3*i+0];
      outpal[i].g = palbase[3*i+1];
      outpal[i].b = palbase[3*i+2];
   }
}

static inline double colourDiff(rgba_t colours[256], rgba_t &rgb, int index)
{
   double d1 = rgb.r - colours[index].r;
   double d2 = rgb.g - colours[index].g;
   double d3 = rgb.b - colours[index].b;
   return (d1*d1)+(d2*d2)+(d3*d3);
}

//
// V_FindNearestColour
//
// From SLADE, but tweaked for PSX in two manners:
// * Default color match should be index 255, not 0.
// * Never match against color 0.
//
int V_FindNearestColour(rgba_t colours[256], rgba_t colour)
{
   double min_d = 999999;
   int    index = 255;
   double delta;

   for(int a = 1; a < 256; a++)
   {
      delta = colourDiff(colours, colour, a);

      // Exact match?
      if(delta == 0.0)
         return a;
      else if(delta < min_d)
      {
         min_d = delta;
         index = a;
      }
   }

   return index;
}

//=============================================================================
//
// COLORMAP Generation
//
// PSX Doom didn't even have a COLORMAP lump at all because it uses hardware
// fog to do light diminishing; the default fade color 0 is black. 8-bit DOOM 
// source ports are going to need a COLORMAP lump however, so we'll generate 
// one here.
//
// NB: Code is largely from SLADE.
// DONE: Modified light fade to more closely match PSX (4/9 factor on level)
//

static uint8_t colormap[34*256];
static float col_greyscale_r = 0.299f;
static float col_greyscale_g = 0.587f;
static float col_greyscale_b = 0.114f;

#define GREENMAP 255
#define GRAYMAP  32
#define DIMINISH(color, level) color = (uint8_t)(((float)color * (32.0f-(4.0f*(float)level/9.0f))+16.0f)/32.0f)

//
// V_GenerateCOLORMAP
//
// Code from SLADE to generate a full set of 34 colormaps from PLAYPAL #0.
//
void V_GenerateCOLORMAP()
{
   rgba_t palette[256];
   rgba_t rgb;
   float  grey;

   V_ColoursFromPLAYPAL(0, palette);

   // Generate 34 maps: the first 32 for diminishing light levels, the 33rd for
   // the inverted grey map used by invulnerability, and the 34th colormap,
   // which remains empty and black.
   for(size_t l = 0; l < 34; l++)
   {
      for(size_t c = 0; c < 256; c++)
      {
         rgb.r = playpal[c*3+0];
         rgb.g = playpal[c*3+1];
         rgb.b = playpal[c*3+2];

         if(l < 32)
         {
            // Generate light maps
            DIMINISH(rgb.r, l);
            DIMINISH(rgb.g, l);
            DIMINISH(rgb.b, l);
         }
         else if(l == GRAYMAP)
         {
            // Generate inverse map
            grey = ((float)rgb.r/256.0f * col_greyscale_r) + 
                   ((float)rgb.g/256.0f * col_greyscale_g) + 
                   ((float)rgb.b/256.0f * col_greyscale_b);
            grey = 1.0f - grey;

            // Clamp value: with id Software's values, the sum is greater than
            // 1.0 (0.299+0.587+0.144=1.030). This means the negation above can
            // give a negative value (for example, with RGB values of 247 or
            // more, which will not be converted correctly to unsigned 8-bit in 
            // the rgba_t structure.
            if(grey < 0.0f)
               grey = 0.0f;
            rgb.r = rgb.g = rgb.b = (uint8_t)(grey*255);
         }
         else
         {
            // Fill with 0
            rgb.r = playpal[0];
            rgb.g = playpal[1];
            rgb.b = playpal[2];
         }

         colormap[256*l+c] = V_FindNearestColour(palette, rgb);
      }
   }
}

//
// V_ConvertCOLORMAPToZip
//
// Write the COLORMAP lump in the root of a zip archive.
//
void V_ConvertCOLORMAPToZip(ziparchive_t *zip)
{
   printf("V_ConvertCOLORMAP: Adding COLORMAP lump.\n");
   Zip_AddFile(zip, "COLORMAP", (byte *)colormap, 34*256, ZIP_FILE_BINARY, false);
}

//=============================================================================
//
// LIGHTS
//
// We convert the LIGHTS lump into a 768-byte palette, as SLADE can
// deal with that format.
//

byte lights[768];

void V_LoadLIGHTS(WadDirectory &dir)
{
   ZAutoBuffer buf;
   dir.cacheLumpAuto("LIGHTS", buf);

   if(buf.getSize() != 1024)
      I_Error("V_LoadLIGHTS: invalid LIGHTS lump!\n");

   auto inptr  = buf.getAs<byte *>();
   auto outptr = lights;
   for(int i = 0; i < 256; i++)
   {
      *outptr++ = *inptr++;
      *outptr++ = *inptr++;
      *outptr++ = *inptr++;
      ++inptr; // discard alpha
   }
}

//
// V_ConvertLIGHTSToZip
//
// Write the LIGHTS lump in the root of a zip archive.
//
void V_ConvertLIGHTSToZip(ziparchive_t *zip)
{
   printf("V_ConvertLIGHTS: Adding PALLIGHT lump.\n");
   Zip_AddFile(zip, "PALLIGHT", lights, 768, ZIP_FILE_BINARY, false);
}

//=============================================================================
//
// CD-XA Extraction
//
// In order to do this, the code below needs access to a raw CD image, so this
// doesn't run inline with the rest of the program. It is an optional
// extraction performed separately - no existing ports are capable of making
// use of the resulting file anyway, but, it can be fed to ffmpeg in order to
// convert it into a watchable/listenable video file - some media players will
// even play it directly without transcoding.
//
// This is necessary because PSX MDEC videos and XA audio channels are written
// to the CD in a pseudo-standard variant of Yellow Book Mode 2 (called CD-XA,
// as mentioned above). Though it has the same sector size, the internal layout
// of the sector is different and discards most of the error correction data
// in a standard Mode 2 sector. Pretty much no PC CD-ROM driver, nor any common
// image mounting software, understands this sector format and either issues IO 
// errors or returns corrupt data if access attempts are made through file
// system APIs. Reading raw from an actual disc or mounted image is additionally
// too complicated; PSX emulators contain thousands of lines of code to do that
// just on Windows alone, using low-level ioctl calls. Out-of-scope for this
// project, so I offer this direct extraction capability as a compromise.
//

// Length in bytes of a Mode 2 Yellow Book CD-ROM sector
#define YB_SECTOR_LEN 2352

// Known offsets of the MOVIE.STR file, in Yellow Book sector numbers, as
// can be retrieved from programs like IsoBuster.
#define NA_OFFSET 822L

// Known lengths of the MOVIE.STR file, in sectors
#define NA_LENGTH 1377L

//
// V_ExtractMovie
//
// Extract raw CD-XA sectors from a CD image file. The default sector at
// which to start extraction is the known offset for the NA release of the
// game. However, the starting sector # and total sector count can be 
// overridden by passing non-zero values in "offset" and "length"
//
void V_ExtractMovie(const qstring &infile, const qstring &outfile, 
                    int offset, int length)
{
   FILE *fin, *fout;
   long sectorOffs = NA_OFFSET; // starting sector #
   long sectorLen  = NA_LENGTH; // number of sectors

   // optional overrides
   if(offset != 0)
      sectorOffs = offset;
   if(length != 0)
      sectorLen = length;

   // multiply offset by Mode 2 sector length
   sectorOffs *= YB_SECTOR_LEN;

   // try to open input
   if(!(fin = fopen(infile.constPtr(), "rb")))
      I_Error("V_ExtractMovie: cannot open CD image file for reading\n");

   // try to open output
   if(!(fout = fopen(outfile.constPtr(), "wb")))
      I_Error("V_ExtractMovie: cannot open output file for write\n");

   // seek to position in the input file
   if(fseek(fin, sectorOffs, SEEK_SET))
      I_Error("V_ExtractMovie: cannot seek to input offset\n");

   long secnum = 0L;
   while(secnum < sectorLen)
   {
      byte buf[YB_SECTOR_LEN];
      size_t bytesRead;
      size_t bytesWrote;

      bytesRead = fread(buf, 1, YB_SECTOR_LEN, fin);

      if(bytesRead)
      {
         bytesWrote = fwrite(buf, 1, bytesRead, fout);
         if(bytesWrote != bytesRead)
         {
            printf(" Warning: failed output file write\n");
            break;
         }
      }

      // premature EOF?
      if(bytesRead != (size_t)YB_SECTOR_LEN)
      {
         printf(" Warning: video file is truncated at sector %ld\n", secnum);
         break;
      }
      ++secnum;
   } 

   fclose(fin);
   fclose(fout);

   printf(" Wrote raw MDEC/XA movie file to '%s'.\n" 
          " Suggested ffmpeg command line for conversion:\n"
          " ffmpeg -r 30 -i %s -c:v libx264 -preset slow -crf 18 -c:a libvorbis\n"
          "  -q:a 5 -pix_fmt yuv420p output.mkv\n",
          outfile.constPtr(), outfile.constPtr());
}

// EOF

