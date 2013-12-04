// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2013 James Haley
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
#include "m_misc.h"
#include "m_swap.h"
#include "r_patch.h"
#include "v_psx.h"
#include "w_wad.h"
#include "z_auto.h"

struct psxpic_header_t
{
   int16_t top;
   int16_t left;
   int16_t width;
   int16_t height;
};

#define PSXPIC_HEADER_SIZE (4*sizeof(int16_t))

#define SAFEUINT16(dest, s) dest = SwapShort(*s | (*(s+1) << 8)); ++read

//
// VPSXImage::readImage
//
// Read the image from a lump.
//
void VPSXImage::readImage(const void *data)
{
   const uint8_t *read = static_cast<const uint8_t *>(data);

   SAFEUINT16(top,    read);
   SAFEUINT16(left,   read);
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
// Constructor
// Taking a lump number in the indicated directory to load.
//
VPSXImage::VPSXImage(WadDirectory &dir, int lumpnum)
{
   ZAutoBuffer buf;
   dir.cacheLumpAuto(lumpnum, buf);
   readImage(buf.get());
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
// PLAYPAL Conversion
//

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

         convPal[3*colnum+0] = (int)((val & 0x001F) >> 0) * (255 /  31);
         convPal[3*colnum+1] = (int)((val & 0x03E0) >> 4) * (255 /  62);
         convPal[3*colnum+2] = (int)((val & 0x7C00) >> 8) * (255 / 124);
      }
      palname[7] = '0' + palnum/10;
      palname[8] = '0' + palnum%10;

      M_WriteFile(palname, convPal, 768);
   }

   // also dump the original lump
   M_WriteFile("PLAYPAL.lmp", buf.get(), buf.getSize());
}

// EOF

