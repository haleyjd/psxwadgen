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
#include "m_compare.h"
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
// Fixes offsets and scaling for weapon sprites.
//
void VPSXImage::adjustOffsets(const char *name)
{
   for(size_t i = 0; i < earrlen(weaponSprites); i++)
   {
      if(!strncasecmp(name, weaponSprites[i], strlen(weaponSprites[i])))
      {
         left = left * 5 / 4;
         left += WEAPON_ORIGIN_X;
         top  += WEAPON_ORIGIN_Y;
         scaleForFourThree();
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

//
// Constructor for sub-image; pulls a rectangular region from the parent
// image and makes it into its own surface.
//
VPSXImage::VPSXImage(const VPSXImage &parent, const rect_t &subrect, 
                     int16_t topoffs, int16_t leftoffs)
{
   // test for subregion validity
   if(subrect.x < 0 || subrect.x + subrect.width  > parent.width ||
      subrect.y < 0 || subrect.y + subrect.height > parent.height)
      I_Error("VPSXImage: invalid subregion for child image\n");
   
   top    = topoffs;
   left   = leftoffs;
   width  = subrect.width;
   height = subrect.height;

   pixels = ecalloc(uint8_t *, width, height);
   mask   = ecalloc(uint8_t *, width, height);
   
   int16_t dsty  = 0;
   int16_t srcy1 = subrect.y;
   int16_t srcy2 = subrect.y + subrect.height;
   do
   {
      uint8_t *psrc = parent.pixels + srcy1 * parent.width + subrect.x;
      uint8_t *msrc = parent.mask   + srcy1 * parent.width + subrect.x;
      uint8_t *pdst = pixels + dsty * width;
      uint8_t *mdst = mask   + dsty * width;
      memcpy(pdst, psrc, width);
      memcpy(mdst, msrc, width);
      ++dsty;
      ++srcy1;
   }
   while(srcy1 != srcy2);
}

//
// Destructor
//
VPSXImage::~VPSXImage()
{
   if(pixels)
      efree(pixels);
   if(mask)
      efree(mask);

   pixels = mask = nullptr;
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

static bool   palettebuilt;
static rgba_t tranpalette[256];

// tranmap with 50% translucency
static byte *tranmap_50;

// tranmap with 25% / 75% translucency
static byte *tranmap_25_75;

static byte V_blendTwoPx25(byte col1, byte col2, byte mask1, byte mask2)
{
   byte mcol1 = mask1 ? col1 : 0;
   byte mcol2 = mask2 ? col2 : 0;

   if(mcol1 && mcol2)
      return tranmap_25_75[(mcol2 << 8) + mcol1];
   else if(mcol2)
      return mcol2;
   else
      return mcol1;
}

static byte V_blendTwoPx75(byte col1, byte col2, byte mask1, byte mask2)
{
   byte mcol1 = mask1 ? col1 : 0;
   byte mcol2 = mask2 ? col2 : 0;

   if(mcol1 && mcol2)
      return tranmap_25_75[(mcol1 << 8) + mcol2];
   else if(mcol1)
      return mcol1;
   else
      return mcol2;
}

static byte V_blendTwoPx50(byte col1, byte col2, byte mask1, byte mask2)
{
   byte mcol1 = mask1 ? col1 : 0;
   byte mcol2 = mask2 ? col2 : 0;

   if(mcol1 && mcol2)
      return tranmap_50[(mcol2 << 8) + mcol1];
   else if(mcol2)
      return mcol2;
   else
      return mcol1;
}

//
// VPSXImage::scaleForFourThree
//
// Upscales the width of a screen patch to account for the 256 -> 320 scaling
// that happened during video signal rasterization on television sets.
//
void VPSXImage::scaleForFourThree()
{
   int scaledWidth = (int)(ceil((width * 5.0) / 4.0));

   if(!palettebuilt)
   {
      V_ColoursFromPLAYPAL(0, tranpalette);
      palettebuilt = true;
   }

   // build tranmaps if not built already
   if(!tranmap_50)
   {
      tranmap_50 = ecalloc(byte *, 256, 256);
      V_BuildTranMap(tranpalette, tranmap_50, 50);
   }
   if(!tranmap_25_75)
   {
      tranmap_25_75 = ecalloc(byte *, 256, 256);
      V_BuildTranMap(tranpalette, tranmap_25_75, 25);
   }

   // allocate upscaled buffer
   byte *newPixels = ecalloc(byte *, scaledWidth, height);
   byte *newMask   = ecalloc(byte *, scaledWidth, height);

   // TEST
   memset(newPixels, 168, scaledWidth*height);

   for(int y = 0; y < height; y++)
   {
      int sx = 0;
      int dx = 0;

      // every 4 source pixels must turn into 5 destination pixels
      for(; sx <= width - 4; sx += 4, dx += 5)
      {
         byte *psrc = pixels    + y * width       + sx;
         byte *pdst = newPixels + y * scaledWidth + dx;
         byte *msrc = mask      + y * width       + sx;
         byte *mdst = newMask   + y * scaledWidth + dx;

         pdst[0] = psrc[0];                                 // 100% 0
         pdst[1] = V_blendTwoPx25(psrc[0], psrc[1], msrc[0], msrc[1]); // 25%  0 + 75% 1
         pdst[2] = V_blendTwoPx50(psrc[1], psrc[2], msrc[1], msrc[2]); // 50%  1 + 50% 2
         pdst[3] = V_blendTwoPx75(psrc[2], psrc[3], msrc[2], msrc[3]); // 75%  2 + 25% 3
         pdst[4] = psrc[3];                                 // 100% 3

         mdst[0] = msrc[0];
         mdst[1] = !!pdst[1];
         mdst[2] = !!pdst[2];
         mdst[3] = !!pdst[3];
         mdst[4] = !!pdst[4];
         mdst[4] = msrc[3];
      }
      if(sx < width) // width is not 0 % 4?
      {
         byte dstpx[] = { 0, 0, 0, 0, 0 };
         byte dstmx[] = { 0, 0, 0, 0, 0 };
         byte *psrc = pixels    + y * width       + sx;
         byte *msrc = mask      + y * width       + sx;
         byte *pdst = newPixels + y * scaledWidth + dx;
         byte *mdst = newMask   + y * scaledWidth + dx;
         
         switch(width % 4)
         {
         case 1: // one pixel left
            dstpx[0] = *psrc;
            dstmx[0] = *msrc;
            break;
         case 2: // two pixels left
            dstpx[0] = *psrc;
            dstmx[0] = *msrc;
            dstpx[1] = V_blendTwoPx25(psrc[0], psrc[1], msrc[0], msrc[1]);
            dstmx[1] = !!dstpx[1];
            dstpx[2] = psrc[1];
            dstmx[2] = msrc[1];
            break;
         case 3: // three pixels left
            dstpx[0] = *psrc;
            dstmx[0] = *msrc;
            dstpx[1] = V_blendTwoPx25(psrc[0], psrc[1], msrc[0], msrc[1]);
            dstmx[1] = !!dstpx[1];
            dstpx[2] = V_blendTwoPx50(psrc[1], psrc[2], msrc[1], msrc[2]);
            dstmx[2] = !!dstpx[2];
            dstpx[3] = psrc[2];
            dstmx[3] = msrc[2];
            break;
         default: // shouldn't be reachable.
            break;
         }
         size_t amt = scaledWidth - dx > 5 ? 5 : scaledWidth - dx;
         if(amt)
         {
            memcpy(pdst, dstpx, amt);
            memcpy(mdst, dstmx, amt);
         }
      }
   }

   // set image to the new pixel array and adjust width
   efree(pixels);
   efree(mask);

   pixels = newPixels;
   mask   = newMask;
   width  = scaledWidth;
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

      // take ownership of the pixel data
      const uint8_t *data = img.releasePixels();
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
// Graphics
//

// The STATUS texture is a 256x256 PSX image containing all of the status bar
// and HUD graphics. This structure and the table of them below holds data on
// how to handle its decomposition into individual resources.
struct statusregion_t
{
   const char *lumpname;   // destination lump
   VPSXImage::rect_t rect; // rectangle on the STATUS texture
   int16_t left;           // left offset
   int16_t top;            // top offset
   bool    noscale;        // if true, don't scale for 4:3
};

#define FACEL(x) -((5 * (28 - (x)) / 4) / 2)
#define FACET(y) -((32 - (y)) / 2)

static statusregion_t StatusRegions[] =
{
   // Status Bar BG
   { "STBAR",    {   0,   0, 256, 32 } }, // should really be 40 tall...

   // Doomguy status bar faces
   { "STFST01",  {   0,  40, 19, 32 }, FACEL(19), FACET(32) }, // o_o
   { "STFST00",  { 234, 136, 19, 32 }, FACEL(19), FACET(32) }, // >_>
   { "STFST02",  {  20,  40, 19, 32 }, FACEL(19), FACET(32) }, // <_<
   { "STFTL00",  {  40,  40, 21, 32 }, FACEL(21), FACET(32) }, // <.<
   { "STFTR00",  {  62,  40, 21, 32 }, FACEL(21), FACET(32) }, // >.>
   { "STFOUCH0", {  84,  40, 19, 32 }, FACEL(19), FACET(32) }, // O.O
   { "STFEVL0",  { 104,  40, 19, 32 }, FACEL(19), FACET(32) }, // >:)
   { "STFKILL0", { 124,  40, 19, 32 }, FACEL(19), FACET(32) }, // >:(
   { "STFST11",  { 144,  40, 19, 32 }, FACEL(19), FACET(32) }, // o_o
   { "STFST10",  { 164,  40, 19, 32 }, FACEL(19), FACET(32) }, // >_>
   { "STFST12",  { 184,  40, 19, 32 }, FACEL(19), FACET(32) }, // <_<
   { "STFTL10",  { 204,  40, 21, 32 }, FACEL(21), FACET(32) }, // <.<
   { "STFTR10",  { 226,  40, 21, 32 }, FACEL(21), FACET(32) }, // >.>
   { "STFOUCH1", {   0,  72, 19, 32 }, FACEL(19), FACET(32) }, // O.O
   { "STFEVL1",  {  20,  72, 19, 32 }, FACEL(19), FACET(32) }, // >:)
   { "STFKILL1", {  40,  72, 19, 32 }, FACEL(19), FACET(32) }, // >:(
   { "STFST21",  {  60,  72, 19, 32 }, FACEL(19), FACET(32) }, // o_o
   { "STFST20",  {  80,  72, 19, 32 }, FACEL(19), FACET(32) }, // >_>
   { "STFST22",  { 100,  72, 19, 32 }, FACEL(19), FACET(32) }, // <_<
   { "STFTL20",  { 120,  72, 22, 32 }, FACEL(22), FACET(32) }, // <.<
   { "STFTR20",  { 142,  72, 23, 32 }, FACEL(23), FACET(32) }, // >.>
   { "STFOUCH2", { 166,  72, 19, 32 }, FACEL(19), FACET(32) }, // O.O
   { "STFEVL2",  { 186,  72, 19, 32 }, FACEL(19), FACET(32) }, // >:)
   { "STFKILL2", { 206,  72, 19, 32 }, FACEL(19), FACET(32) }, // >:(
   { "STFST31",  { 226,  72, 19, 32 }, FACEL(19), FACET(32) }, // o_o
   { "STFST30",  {   0, 104, 19, 32 }, FACEL(19), FACET(32) }, // >_>
   { "STFST32",  {  20, 104, 19, 32 }, FACEL(19), FACET(32) }, // <_<
   { "STFTL30",  {  40, 104, 23, 32 }, FACEL(23), FACET(32) }, // <.<
   { "STFTR30",  {  64, 104, 23, 32 }, FACEL(23), FACET(32) }, // >.>
   { "STFOUCH3", {  88, 104, 19, 32 }, FACEL(19), FACET(32) }, // O.O
   { "STFEVL3",  { 108, 104, 19, 32 }, FACEL(19), FACET(32) }, // >:)
   { "STFKILL3", { 128, 104, 19, 32 }, FACEL(19), FACET(32) }, // >:(
   { "STFST41",  { 148, 104, 19, 32 }, FACEL(19), FACET(32) }, // o_o
   { "STFST40",  { 168, 104, 19, 32 }, FACEL(19), FACET(32) }, // >_>
   { "STFST42",  { 188, 104, 19, 32 }, FACEL(19), FACET(32) }, // <_<
   { "STFTL40",  { 208, 104, 24, 32 }, FACEL(24), FACET(32) }, // <.<
   { "STFTR40",  { 232, 104, 23, 32 }, FACEL(23), FACET(32) }, // >.>
   { "STFOUCH4", {   0, 136, 19, 32 }, FACEL(19), FACET(32) }, // O.O
   { "STFEVL4",  {  20, 136, 19, 32 }, FACEL(19), FACET(32) }, // >:)
   { "STFKILL4", {  40, 136, 19, 32 }, FACEL(19), FACET(32) }, // >:(
   { "STFGOD0",  {  60, 136, 19, 32 }, FACEL(19), FACET(32) }, // O___O
   { "STFDEAD0", {  80, 136, 19, 32 }, FACEL(19), FACET(32) }, // -_-
   { "STFGIBS0", { 100, 136, 19, 32 }, FACEL(19), FACET(32) }, // x_x
   { "STFGIBS1", { 122, 137, 25, 30 }, FACEL(25), FACET(30) }, // . _ X
   { "STFGIBS2", { 148, 137, 26, 30 }, FACEL(26), FACET(30) }, // .  / X
   { "STFGIBS3", { 174, 137, 30, 30 }, FACEL(30), FACET(30) }, // .(  )X
   { "STFGIBS4", { 204, 137, 28, 30 }, FACEL(28), FACET(30) }, // *SPLORTCH*

   // "small" HUD font characters
   { "STCFN033", {   0, 168,  8,  8 } }, // !
   { "STCFN034", {   8, 168,  8,  8 } }, // "
   { "STCFN035", {  16, 168,  8,  8 } }, // #
   { "STCFN036", {  24, 168,  8,  8 } }, // $
   { "STCFN037", {  32, 168,  8,  8 } }, // %
   { "STCFN038", {  40, 168,  8,  8 } }, // &
   { "STCFN039", {  48, 168,  8,  8 } }, // '
   { "STCFN040", {  56, 168,  8,  8 } }, // (
   { "STCFN041", {  64, 168,  8,  8 } }, // )
   { "STCFN042", {  72, 168,  8,  8 } }, // *
   { "STCFN043", {  80, 168,  8,  8 } }, // +
   { "STCFN044", {  88, 168,  8,  8 } }, // ,
   { "STCFN045", {  96, 168,  8,  8 } }, // -
   { "STCFN046", { 104, 168,  8,  8 } }, // .
   { "STCFN047", { 112, 168,  8,  8 } }, // slash
   { "STCFN048", { 120, 168,  8,  8 } }, // 0
   { "STCFN049", { 128, 168,  8,  8 } }, // 1
   { "STCFN050", { 136, 168,  8,  8 } }, // 2
   { "STCFN051", { 144, 168,  8,  8 } }, // 3
   { "STCFN052", { 152, 168,  8,  8 } }, // 4
   { "STCFN053", { 160, 168,  8,  8 } }, // 5
   { "STCFN054", { 168, 168,  8,  8 } }, // 6
   { "STCFN055", { 176, 168,  8,  8 } }, // 7
   { "STCFN056", { 184, 168,  8,  8 } }, // 8
   { "STCFN057", { 192, 168,  8,  8 } }, // 9
   { "STCFN058", { 200, 168,  8,  8 } }, // :
   { "STCFN059", { 208, 168,  8,  8 } }, // ;
   { "STCFN060", { 216, 168,  8,  8 } }, // <
   { "STCFN061", { 224, 168,  8,  8 } }, // =
   { "STCFN062", { 232, 168,  8,  8 } }, // >
   { "STCFN063", { 240, 168,  8,  8 } }, // ?
   { "STCFN064", { 248, 168,  8,  8 } }, // @
   { "STCFN065", {   0, 176,  8,  8 } }, // A
   { "STCFN066", {   8, 176,  8,  8 } }, // B
   { "STCFN067", {  16, 176,  8,  8 } }, // C
   { "STCFN068", {  24, 176,  8,  8 } }, // D
   { "STCFN069", {  32, 176,  8,  8 } }, // E
   { "STCFN070", {  40, 176,  8,  8 } }, // F
   { "STCFN071", {  48, 176,  8,  8 } }, // G
   { "STCFN072", {  56, 176,  8,  8 } }, // H
   { "STCFN073", {  64, 176,  8,  8 } }, // I
   { "STCFN074", {  72, 176,  8,  8 } }, // J
   { "STCFN075", {  80, 176,  8,  8 } }, // K
   { "STCFN076", {  88, 176,  8,  8 } }, // L
   { "STCFN077", {  96, 176,  9,  8 } }, // M
   { "STCFN078", { 104, 176,  8,  8 } }, // N
   { "STCFN079", { 112, 176,  8,  8 } }, // O
   { "STCFN080", { 120, 176,  8,  8 } }, // P
   { "STCFN081", { 128, 176,  8,  8 } }, // Q
   { "STCFN082", { 136, 176,  8,  8 } }, // R
   { "STCFN083", { 144, 176,  8,  8 } }, // S
   { "STCFN084", { 152, 176,  8,  8 } }, // T
   { "STCFN085", { 160, 176,  8,  8 } }, // U
   { "STCFN086", { 168, 176,  8,  8 } }, // V
   { "STCFN087", { 176, 176,  9,  8 } }, // W
   { "STCFN088", { 184, 176,  8,  8 } }, // X
   { "STCFN089", { 192, 176,  8,  8 } }, // Y
   { "STCFN090", { 200, 176,  8,  8 } }, // Z
   { "STCFN091", { 208, 176,  8,  8 } }, // [
   { "STCFN092", { 216, 176,  8,  8 } }, // backslash
   { "STCFN093", { 224, 176,  8,  8 } }, // ]
   { "STCFN094", { 232, 176,  8,  8 } }, // ^
   { "STCFN095", { 240, 176,  8,  8 } }, // _
   { "STCFN121", {  64, 176,  8,  8 } }, // | (duplicate of I character)

   // copy of HUD font characters for menu
   { "MNUFN033", {   2, 168,  4,  8 }, 0, 0, true }, // !
   { "MNUFN034", {   8, 168,  7,  8 }, 0, 0, true }, // "
   { "MNUFN035", {  16, 168,  7,  8 }, 0, 0, true }, // #
   { "MNUFN036", {  24, 168,  7,  8 }, 0, 0, true }, // $
   { "MNUFN037", {  32, 168,  7,  8 }, 0, 0, true }, // %
   { "MNUFN038", {  40, 168,  8,  8 }, 0, 0, true }, // &
   { "MNUFN039", {  48, 168,  4,  8 }, 0, 0, true }, // '
   { "MNUFN040", {  56, 168,  7,  8 }, 0, 0, true }, // (
   { "MNUFN041", {  64, 168,  7,  8 }, 0, 0, true }, // )
   { "MNUFN042", {  72, 168,  7,  8 }, 0, 0, true }, // *
   { "MNUFN043", {  81, 168,  5,  8 }, 0, 0, true }, // +
   { "MNUFN044", {  89, 168,  4,  8 }, 0, 0, true }, // ,
   { "MNUFN045", {  97, 168,  6,  8 }, 0, 0, true }, // -
   { "MNUFN046", { 104, 168,  4,  8 }, 0, 0, true }, // .
   { "MNUFN047", { 112, 168,  7,  8 }, 0, 0, true }, // slash
   { "MNUFN048", { 120, 168,  8,  8 }, 0, 0, true }, // 0
   { "MNUFN049", { 129, 168,  5,  8 }, 0, 0, true }, // 1
   { "MNUFN050", { 136, 168,  8,  8 }, 0, 0, true }, // 2
   { "MNUFN051", { 144, 168,  8,  8 }, 0, 0, true }, // 3
   { "MNUFN052", { 152, 168,  7,  8 }, 0, 0, true }, // 4
   { "MNUFN053", { 160, 168,  7,  8 }, 0, 0, true }, // 5
   { "MNUFN054", { 168, 168,  8,  8 }, 0, 0, true }, // 6
   { "MNUFN055", { 176, 168,  8,  8 }, 0, 0, true }, // 7
   { "MNUFN056", { 184, 168,  8,  8 }, 0, 0, true }, // 8
   { "MNUFN057", { 192, 168,  8,  8 }, 0, 0, true }, // 9
   { "MNUFN058", { 200, 168,  4,  8 }, 0, 0, true }, // :
   { "MNUFN059", { 208, 168,  4,  8 }, 0, 0, true }, // ;
   { "MNUFN060", { 216, 168,  5,  8 }, 0, 0, true }, // <
   { "MNUFN061", { 225, 168,  5,  8 }, 0, 0, true }, // =
   { "MNUFN062", { 232, 168,  5,  8 }, 0, 0, true }, // >
   { "MNUFN063", { 240, 168,  8,  8 }, 0, 0, true }, // ?
   { "MNUFN064", { 248, 168,  8,  8 }, 0, 0, true }, // @
   { "MNUFN065", {   0, 176,  8,  8 }, 0, 0, true }, // A
   { "MNUFN066", {   8, 176,  8,  8 }, 0, 0, true }, // B
   { "MNUFN067", {  16, 176,  8,  8 }, 0, 0, true }, // C
   { "MNUFN068", {  24, 176,  8,  8 }, 0, 0, true }, // D
   { "MNUFN069", {  32, 176,  8,  8 }, 0, 0, true }, // E
   { "MNUFN070", {  40, 176,  8,  8 }, 0, 0, true }, // F
   { "MNUFN071", {  48, 176,  8,  8 }, 0, 0, true }, // G
   { "MNUFN072", {  56, 176,  8,  8 }, 0, 0, true }, // H
   { "MNUFN073", {  66, 176,  4,  8 }, 0, 0, true }, // I
   { "MNUFN074", {  72, 176,  8,  8 }, 0, 0, true }, // J
   { "MNUFN075", {  80, 176,  8,  8 }, 0, 0, true }, // K
   { "MNUFN076", {  88, 176,  8,  8 }, 0, 0, true }, // L
   { "MNUFN077", {  96, 176,  9,  8 }, 0, 0, true }, // M
   { "MNUFN078", { 104, 176,  8,  8 }, 0, 0, true }, // N
   { "MNUFN079", { 112, 176,  8,  8 }, 0, 0, true }, // O
   { "MNUFN080", { 120, 176,  8,  8 }, 0, 0, true }, // P
   { "MNUFN081", { 128, 176,  8,  8 }, 0, 0, true }, // Q
   { "MNUFN082", { 136, 176,  8,  8 }, 0, 0, true }, // R
   { "MNUFN083", { 144, 176,  7,  8 }, 0, 0, true }, // S
   { "MNUFN084", { 152, 176,  8,  8 }, 0, 0, true }, // T
   { "MNUFN085", { 160, 176,  8,  8 }, 0, 0, true }, // U
   { "MNUFN086", { 168, 176,  7,  8 }, 0, 0, true }, // V
   { "MNUFN087", { 176, 176,  9,  8 }, 0, 0, true }, // W
   { "MNUFN088", { 185, 176,  7,  8 }, 0, 0, true }, // X
   { "MNUFN089", { 192, 176,  8,  8 }, 0, 0, true }, // Y
   { "MNUFN090", { 200, 176,  7,  8 }, 0, 0, true }, // Z
   { "MNUFN091", { 208, 176,  5,  8 }, 0, 0, true }, // [
   { "MNUFN092", { 216, 176,  7,  8 }, 0, 0, true }, // backslash
   { "MNUFN093", { 224, 176,  5,  8 }, 0, 0, true }, // ]
   { "MNUFN094", { 232, 176,  7,  8 }, 0, 0, true }, // ^
   { "MNUFN095", { 240, 176,  8,  8 }, 0, 0, true }, // _
   { "MNUFN121", {  66, 176,  4,  8 }, 0, 0, true }, // | (duplicate of I character)

   // Statbar key icons
   { "STKEYS0",  { 125, 184, 11,  8 } }, // Blue Card
   { "STKEYS1",  { 136, 184, 11,  8 } }, // Yellow Card
   { "STKEYS2",  { 114, 184, 11,  8 } }, // Red Card
   { "STKEYS3",  { 158, 184, 11,  8 } }, // Blue Skull
   { "STKEYS4",  { 169, 184, 11,  8 } }, // Yellow Skull
   { "STKEYS5",  { 147, 184, 11,  8 } }, // Red Skull

   // Statbar/intermission/big font numbers
   { "FONTB63",  { 232, 190, 10, 16 } }, // _
   { "WINUM0",   {   0, 195, 12, 16 } }, // 0
   { "WINUM1",   {  12, 195, 12, 16 } }, // 1
   { "WINUM2",   {  24, 195, 12, 16 } }, // 2
   { "WINUM3",   {  36, 195, 12, 16 } }, // 3
   { "WINUM4",   {  48, 195, 12, 16 } }, // 4
   { "WINUM5",   {  60, 195, 12, 16 } }, // 5
   { "WINUM6",   {  72, 195, 12, 16 } }, // 6
   { "WINUM7",   {  84, 195, 12, 16 } }, // 7
   { "WINUM8",   {  96, 195, 12, 16 } }, // 8
   { "WINUM9",   { 108, 195, 12, 16 } }, // 9
   { "WIPCNT",   { 120, 195, 12, 16 } }, // %
   { "WIMINUS",  { 232, 199, 10, 16 } }, // -
   { "STTNUM0",  {   0, 195, 12, 16 } }, // 0
   { "STTNUM1",  {  12, 195, 12, 16 } }, // 1
   { "STTNUM2",  {  24, 195, 12, 16 } }, // 2
   { "STTNUM3",  {  36, 195, 12, 16 } }, // 3
   { "STTNUM4",  {  48, 195, 12, 16 } }, // 4
   { "STTNUM5",  {  60, 195, 12, 16 } }, // 5
   { "STTNUM6",  {  72, 195, 12, 16 } }, // 6
   { "STTNUM7",  {  84, 195, 12, 16 } }, // 7
   { "STTNUM8",  {  96, 195, 12, 16 } }, // 8
   { "STTNUM9",  { 108, 195, 12, 16 } }, // 9
   { "STTPRCNT", { 120, 195, 12, 16 } }, // %
   { "STTMINUS", { 232, 199, 10, 16 } }, // -
   { "FONTB16",  {   0, 195, 12, 16 } }, // 0
   { "FONTB17",  {  12, 195, 12, 16 } }, // 1
   { "FONTB18",  {  24, 195, 12, 16 } }, // 2
   { "FONTB19",  {  36, 195, 12, 16 } }, // 3
   { "FONTB20",  {  48, 195, 12, 16 } }, // 4
   { "FONTB21",  {  60, 195, 12, 16 } }, // 5
   { "FONTB22",  {  72, 195, 12, 16 } }, // 6
   { "FONTB23",  {  84, 195, 12, 16 } }, // 7
   { "FONTB24",  {  96, 195, 12, 16 } }, // 8
   { "FONTB25",  { 108, 195, 12, 16 } }, // 9
   { "FONTB05",  { 120, 195, 12, 16 } }, // %
   { "FONTB13",  { 232, 199, 10, 16 } }, // -

   // Skull cursor
   { "M_SKULL1", { 132, 192, 16, 18 } },
   { "M_SKULL2", { 148, 192, 16, 18 } },

   // FONTB mainline chars
   { "FONTB01",  {   0, 211,  8, 16 } }, // !
   { "FONTB14",  {   8, 211,  8, 16 } }, // .
   { "FONTB33",  {  16, 211, 16, 16 } }, // A
   { "FONTB34",  {  32, 211, 14, 16 } }, // B
   { "FONTB35",  {  46, 211, 14, 16 } }, // C
   { "FONTB36",  {  60, 211, 14, 16 } }, // D
   { "FONTB37",  {  74, 211, 14, 16 } }, // E
   { "FONTB38",  {  88, 211, 14, 16 } }, // F
   { "FONTB39",  { 102, 211, 14, 16 } }, // G
   { "FONTB40",  { 116, 211, 14, 16 } }, // H
   { "FONTB41",  { 130, 211,  7, 16 } }, // I
   { "FONTB42",  { 137, 211, 11, 16 } }, // J
   { "FONTB43",  { 148, 211, 14, 16 } }, // K
   { "FONTB44",  { 162, 211, 14, 16 } }, // L
   { "FONTB45",  { 176, 211, 16, 16 } }, // M
   { "FONTB46",  { 192, 211, 16, 16 } }, // N
   { "FONTB47",  { 208, 211, 14, 16 } }, // O
   { "FONTB48",  { 222, 211, 14, 16 } }, // P
   { "FONTB49",  { 236, 211, 14, 16 } }, // Q
   { "FONTB50",  {   0, 227, 14, 16 } }, // R
   { "FONTB51",  {  14, 227, 14, 16 } }, // S
   { "FONTB52",  {  28, 227, 14, 16 } }, // T
   { "FONTB53",  {  42, 227, 14, 16 } }, // U
   { "FONTB54",  {  56, 227, 16, 16 } }, // V
   { "FONTB55",  {  72, 227, 16, 16 } }, // W
   { "FONTB56",  {  88, 227, 16, 16 } }, // X
   { "FONTB57",  { 104, 227, 14, 16 } }, // Y
   { "FONTB58",  { 118, 227, 14, 16 } }, // Z
   { "FONTB65",  { 132, 230, 14, 13 }, 0, -3 }, // a
   { "FONTB66",  { 146, 230, 12, 13 }, 0, -3 }, // b
   { "FONTB67",  { 158, 230, 12, 13 }, 0, -3 }, // c
   { "FONTB68",  { 170, 230, 12, 13 }, 0, -3 }, // d
   { "FONTB69",  { 182, 230, 10, 13 }, 0, -3 }, // e
   { "FONTB70",  { 192, 230, 11, 13 }, 0, -3 }, // f
   { "FONTB71",  { 204, 230, 12, 13 }, 0, -3 }, // g
   { "FONTB72",  { 216, 230, 12, 13 }, 0, -3 }, // h
   { "FONTB73",  { 228, 230,  6, 13 }, 0, -3 }, // i
   { "FONTB74",  { 234, 230, 10, 13 }, 0, -3 }, // j
   { "FONTB75",  {   0, 243, 12, 13 }, 0, -3 }, // k
   { "FONTB76",  {  12, 243, 10, 13 }, 0, -3 }, // l
   { "FONTB77",  {  22, 243, 14, 13 }, 0, -3 }, // m
   { "FONTB78",  {  36, 243, 14, 13 }, 0, -3 }, // n
   { "FONTB79",  {  50, 243, 12, 13 }, 0, -3 }, // o
   { "FONTB80",  {  62, 243, 12, 13 }, 0, -3 }, // p
   { "FONTB81",  {  74, 243, 12, 13 }, 0, -3 }, // q
   { "FONTB82",  {  86, 243, 12, 13 }, 0, -3 }, // r
   { "FONTB83",  {  98, 243, 13, 13 }, 0, -3 }, // s
   { "FONTB84",  { 112, 243, 12, 13 }, 0, -3 }, // t
   { "FONTB85",  { 124, 243, 12, 13 }, 0, -3 }, // u
   { "FONTB86",  { 136, 243, 14, 13 }, 0, -3 }, // v
   { "FONTB87",  { 150, 243, 14, 13 }, 0, -3 }, // w
   { "FONTB88",  { 164, 243, 14, 13 }, 0, -3 }, // x
   { "FONTB89",  { 178, 243, 14, 13 }, 0, -3 }, // y
   { "FONTB90",  { 192, 243, 14, 13 }, 0, -3 }  // z
};

//
// V_convertSTATUSToZip  
//
// Load and then decomposite the STATUS texture lump into its components
// and add them to a ZIP archive.
//
static void V_convertSTATUSToZip(WadDirectory &dir, ziparchive_t *zip)
{
   VPSXImage statusImg(dir, "STATUS");

   printf("* Converting STATUS texture...\n");

   for(size_t i = 0; i < earrlen(StatusRegions); i++)
   {
      statusregion_t &reg = StatusRegions[i];
      VPSXImage subImg(statusImg, reg.rect, reg.top, reg.left);

      if(!reg.noscale)
         subImg.scaleForFourThree();

      size_t  size = 0;
      void   *data = subImg.toPatch(size);
      qstring name;

      name << "graphics/" << reg.lumpname;

      Zip_AddFile(zip, name.constPtr(), (byte *)data, (uint32_t)size, 
                  ZIP_FILE_BINARY, true);
   }
}

struct screen_t
{
   const char *psxLumpName;
   const char *destLumpName;
};

static screen_t screens[] =
{
   { "BACK",  "INTERPIC" }, // intermission background
   { "DEMON", "BOSSBACK" }, // boss background
};

//
// V_convertScreensToZip
//
// Put in the miscellaneous screen lumps.
//
static void V_convertScreensToZip(WadDirectory &dir, ziparchive_t *zip)
{
   printf("* Converting screens...\n");

   for(size_t i = 0; i < earrlen(screens); i++)
   {
      VPSXImage screen(dir, screens[i].psxLumpName);
      screen.scaleForFourThree();

      uint32_t  size = screen.getWidth() * screen.getHeight(); 
      uint8_t  *pic  = screen.releasePixels();
      qstring   name;

      name << "graphics/" << screens[i].destLumpName;

      Zip_AddFile(zip, name.constPtr(), pic, size, 
                  ZIP_FILE_BINARY, true);
   }
}

//
// V_ConvertGraphicsToZip
//
// Write out screen graphic lumps to a ZIP archive.
//
void V_ConvertGraphicsToZip(WadDirectory &dir, ziparchive_t *zip)
{
   printf("V_ConvertGraphics: converting graphics:\n");

   // create graphics directory
   Zip_AddFile(zip, "graphics/", NULL, 0, ZIP_DIRECTORY, false);

   // decomposite STATUS texture
   V_convertSTATUSToZip(dir, zip);

   // convert screens
   V_convertScreensToZip(dir, zip);
}

//=============================================================================
//
// PLAYPAL Conversion
//

// playpal data in RGB 24-bit format
static byte   *playpal;
static size_t  numpals;

//
// V_RGBToHSL
//
// Convert RGB color to HSL color space. From SLADE.
//
static hsl_t V_RGBToHSL(const rgba_t &rgb)
{
   hsl_t ret;

   double r, g, b;
   r = (double)rgb.r / 255.0f;
   g = (double)rgb.g / 255.0f;
   b = (double)rgb.b / 255.0f;

   double v_min = emin(r, emin(g, b));
   double v_max = emax(r, emax(g, b));
   double delta = v_max - v_min;

   // determine V
   ret.l = (v_max + v_min) * 0.5;

   if(delta == 0)
      ret.h = ret.s = 0; // gray (r == g == b)
   else
   {
      // determine s
      if(ret.l < 0.5)
         ret.s = delta / (v_max + v_min);
      else
         ret.s = delta / (2.0 - v_max - v_min);

      // determine h
      if(r == v_max)
         ret.h = (g - b) / delta;
      else if(g == v_max)
         ret.h = 2.0 + (b - r) / delta;
      else if(b == v_max)
         ret.h = 4.0 + (r - g) / delta;

      ret.h /= 6.0;
      if(ret.h < 0)
         ret.h += 1.0;
   }
   return ret;
}

//
// V_HSLToRGB
//
// Convert HSL color to RGB color space. From SLADE.
//
static rgba_t V_HSLToRGB(const hsl_t &hsl)
{
   edefstructvar(rgba_t, ret);
   ret.a = 255;

   // no saturation means gray
   if(hsl.s == 0.0)
   {
      ret.r = ret.g = ret.b = (uint8_t)(255.0 * hsl.l);
      return ret;
   }

   // Find the rough values at given H with mid L and max S.
   double  hue    = (6.0 * hsl.h);
   uint8_t sector = (uint8_t)hue;
   double  factor = hue - sector;
   double  dr, dg, db;
   switch(sector)
   {
   case 0: // RGB 0xFF0000 to 0xFFFF00, increasingly green
      dr = 1.0; dg = factor; db = 0.0; 
      break;
   case 1: // RGB 0xFFFF00 to 0x00FF00, decreasingly red
      dr = 1.0 - factor; dg = 1.0; db = 0.0; 
      break;
   case 2: // RGB 0x00FF00 to 0x00FFFF, increasingly blue
      dr = 0.0; dg = 1.0; db = factor; 
      break;
   case 3: // RGB 0x00FFFF to 0x0000FF, decreasingly green
      dr = 0.0; dg = 1.0 - factor; db = 1.0; 
      break;
   case 4: // RGB 0x0000FF to 0xFF00FF, increasingly red
      dr = factor; dg = 0.0; db = 1.0; 
      break;
   case 5: // RGB 0xFF00FF to 0xFF0000, decreasingly blue
      dr = 1.0; dg = 0.0; db = 1.0 - factor; 
      break;
   }

   // Now apply desaturation
   double ds = (1.0 - hsl.s) * 0.5;
   dr = ds + (dr * hsl.s);
   dg = ds + (dg * hsl.s);
   db = ds + (db * hsl.s);

   // Finally apply luminosity
   double dl = hsl.l * 2.0;
   double sr, sg, sb, sl;
   if(dl > 1.0)
   {
      // Make brighter
      sl = dl - 1.0;
      sr = sl * (1.0 - dr); dr += sr;
      sg = sl * (1.0 - dg); dg += sg;
      sb = sl * (1.0 - db); db += sb;
   }
   else if (dl < 1.0)
   {
      // Make darker
      sl = 1.0 - dl;
      sr = sl * dr; dr -= sr;
      sg = sl * dg; dg -= sg;
      sb = sl * db; db -= sb;
   }

   // Clamping (shouldn't actually be needed)
   if(dr > 1.0) dr = 1.0; if (dr < 0.0) dr = 0.0;
   if(dg > 1.0) dg = 1.0; if (dg < 0.0) dg = 0.0;
   if(db > 1.0) db = 1.0; if (db < 0.0) db = 0.0;

   // Now convert from 0f--1f to 0i--255i, rounding up
   ret.r = (uint8_t)(dr * 255.0 + 0.499999999);
   ret.g = (uint8_t)(dg * 255.0 + 0.499999999);
   ret.b = (uint8_t)(db * 255.0 + 0.499999999);

   return ret;
}

// LOG TEST
/*
static byte V_logColor(byte incolor)
{
   float fcolor   = (float)incolor;
   float colScale = fcolor * 10.0f / 255.0f;

   return (byte)(incolor * log10(colScale));
}
*/

static void V_logColor(byte &r, byte &g, byte &b)
{
   rgba_t rgb;
   rgb.r = r;
   rgb.g = g;
   rgb.b = b;
   rgb.a = 255;

   hsl_t hsl = V_RGBToHSL(rgb);
   //hsl.l *= log10((hsl.l + 1.0) * 5);
   hsl.l *= log10(hsl.l * 9.0 + 1.0);

   rgb = V_HSLToRGB(hsl);
   r = rgb.r;
   g = rgb.g;
   b = rgb.b;
}

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

         palptr[3*colnum+0] = (byte)(((val & 0x001F) << 3)/* + 7*/);
         palptr[3*colnum+1] = (byte)(((val & 0x03E0) >> 2)/* + 7*/);
         palptr[3*colnum+2] = (byte)(((val & 0x7C00) >> 7)/* + 7*/);

         //V_logColor(palptr[3*colnum+0], palptr[3*colnum+1], palptr[3*colnum+2]);
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
// Tranmap computation (for internal use)
//

// number of fixed point digits in filter percent
#define TSC 12 

void V_BuildTranMap(rgba_t colours[256], byte *map, int pct)
{
   int pal[3][256], tot[256], pal_w1[3][256];
   int w1 = ((unsigned int) pct << TSC) / 100;
   int w2 = (1l << TSC) - w1;
   int i;

   // First, convert playpal into long int type, and transpose array,
   // for fast inner-loop calculations. Precompute tot array.
   i = 255;
   do
   {
      int t, d;
      pal_w1[0][i] = (pal[0][i] = t = colours[i].r) * w1;
      d = t*t;
      pal_w1[1][i] = (pal[1][i] = t = colours[i].g) * w1;
      d += t*t;
      pal_w1[2][i] = (pal[2][i] = t = colours[i].b) * w1;
      d += t*t;
      tot[i] = d << (TSC - 1);
   }
   while(--i >= 0);

   // Next, compute all entries using minimum arithmetic.
   byte *tp = map;
   for(i = 0; i < 256; i++)
   {
      int r1 = pal[0][i] * w2;
      int g1 = pal[1][i] * w2;
      int b1 = pal[2][i] * w2;

      for(int j = 0; j < 256; j++, tp++)
      {
         int color = 255;
         int err;
         int r = pal_w1[0][j] + r1;
         int g = pal_w1[1][j] + g1;
         int b = pal_w1[2][j] + b1;
         int best = INT_MAX;
         do
         {
            if((err = tot[color] - pal[0][color]*r
               - pal[1][color]*g - pal[2][color]*b) < best)
               best = err, *tp = color;
         }
         while(--color >= 1); // don't match against color 0
      }
   }
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
//

static uint8_t colormap[34*256];
static float col_greyscale_r = 0.299f;
static float col_greyscale_g = 0.587f;
static float col_greyscale_b = 0.114f;

#define GREENMAP 255
#define GRAYMAP  32

#define DIMINISH(color, level) color = (uint8_t)(((float)color * (32.0f - (float)level) + 16.0f)/32.0f)
//#define DIMINISH(color, level) color = (uint8_t)(((float)color * (32.0f-(5.0f*(float)level/9.0f))+16.0f)/32.0f)


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

