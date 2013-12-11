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
//    Zip file writer
//
//-----------------------------------------------------------------------------

#include "z_zone.h"

#include "i_system.h"
#include "m_buffer.h"
#include "v_loading.h"
#include "z_auto.h"
#include "zip_write.h"

// Need zlib for deflate support
#include "zlib/zlib.h"

//=============================================================================
//
// Zip Functions
//

//
// Zip_Create
//
// Initialize a ziparchive_t structure.
// 
void Zip_Create(ziparchive_t *zip, const char *filename)
{
   memset(zip, 0, sizeof(ziparchive_t));
   zip->files = zip->last = NULL;
   zip->filename = filename;
}

//
// Zip_AddFile
//
// Add a file to a zip archive.
//
zipfile_t *Zip_AddFile(ziparchive_t *zip, const char *name, const byte *data, 
                       uint32_t len, ziptype_e fileType, bool deflate)
{
   auto file = estructalloc(zipfile_t, 1);

   file->name    = estrdup(name);
   file->data    = data;
   file->len     = len;
   file->clen    = len;     // start out clen same as len
   file->deflate = deflate;

   // Does anything actually pay attention to these? Oh well.
   switch(fileType)
   {
   case ZIP_FILE_TEXT:
      file->intattr = 0x01; // is a text file
      file->extattr = 0x20; // set archive flag
      break;
   case ZIP_FILE_BINARY:
      file->extattr = 0x20; // set archive flag
      break;
   case ZIP_DIRECTORY:
      file->extattr = 0x10; // set directory flag
      break;
   default:
      break;
   }

   if(zip->last)
   {
      zip->last->next = file;
      zip->last = file;
   }
   else
      zip->files = zip->last = file;

   zip->fcount++;

   return file;
}

//=============================================================================
//
// CRC routine
//

#define CRC32_IEEE_POLY 0xEDB88320

static uint32_t crc32_table[256];

//
// M_CRC32BuildTable
//
// Builds the polynomial table for CRC32.
//
static void M_CRC32BuildTable()
{
   uint32_t i, j;
   
   for(i = 0; i < 256; i++)
   {
      uint32_t val = i;
      
      for(j = 0; j < 8; j++)
      {
         if(val & 1)
            val = (val >> 1) ^ CRC32_IEEE_POLY;
         else
            val >>= 1;
      }
      
      crc32_table[i] = val;
   }
}

//
// M_CRC32Initialize
//
// Builds the table if it hasn't been built yet. Doesn't do anything
// else, since CRC32 starting value has already been set properly by
// the main hash init routine.
//
static void M_CRC32Initialize()
{
   static bool tablebuilt = false;
   
   // build the CRC32 table if it hasn't been built yet
   if(!tablebuilt)
   {
      M_CRC32BuildTable();
      tablebuilt = true;
   }
   
   // zero is the appropriate starting value for CRC32, so we need do nothing
   // special here
}

//
// M_CRC32HashData
//
// Calculates a running CRC32 for the provided block of data.
//
static uint32_t M_CRC32HashData(const byte *data, unsigned int len)
{
   uint32_t crc = 0xFFFFFFFF;
   
   while(len)
   {
      byte idx = (byte)(((int)crc ^ *data++) & 0xff);
      
      crc = crc32_table[idx] ^ (crc >> 8);
      
      --len;
   }
   
   return crc ^ 0xFFFFFFFF;
}

//
// End CRC routine
//
//=============================================================================

//
// Zip_Compress
//
// Customized version of the zlib compress() routine which is suitable for use
// with PKZIP archives (need to set window bits to -MAX_WBITS so that we get
// a raw deflate stream, for one).
//
static int Zip_Compress(Bytef *dest, uLongf *destLen, const Bytef *source,
                        uLong sourceLen)
{
   z_stream stream;
   int err;

   stream.next_in   = (Bytef*)source;
   stream.avail_in  = (uInt)sourceLen;
   stream.next_out  = dest;
   stream.avail_out = (uInt)*destLen;
   
   if((uLong)stream.avail_out != *destLen)
      return Z_BUF_ERROR;

   stream.zalloc = Z_NULL;
   stream.zfree  = Z_NULL;
   stream.opaque = Z_NULL;

   err = deflateInit2(&stream, Z_BEST_COMPRESSION, Z_DEFLATED, 
                      -MAX_WBITS, MAX_MEM_LEVEL, Z_DEFAULT_STRATEGY);
   if(err != Z_OK)
      return err;

   err = deflate(&stream, Z_FINISH);
   if(err != Z_STREAM_END) 
   {
      deflateEnd(&stream);
      return err == Z_OK ? Z_BUF_ERROR : err;
   }
   *destLen = stream.total_out;

   err = deflateEnd(&stream);
   return err;
}

//
// Zip_WriteFile
//
// Write the local file header and file data for a single entry in the zip
// archive.
//
static void Zip_WriteFile(zipfile_t *file, OutBuffer &ob)
{
   ZAutoBuffer buffer;
   uint16_t    date, time;
   uint16_t    namelen;
   const byte *data;
   uint32_t    len;

   // Can't deflate an empty file
   if(!file->len || !file->data)
      file->deflate = false;

   ob.Flush();

   file->offset = ob.Tell();

   ob.WriteUint32(0x04034b50); // local file header signature
   ob.WriteUint16(0x14);       // version needed to extract (2.0)

   // general purpose bit flag and compression method
   if(file->deflate)
   {
      ob.WriteUint16(2); // Max deflate compressed
      ob.WriteUint16(8); // compression method == deflate
   }
   else
   {
      ob.WriteUint16(0); // Nothing special 
      ob.WriteUint16(0); // compression method == store
   }
   
   // Time is psxwadgen version #; date is PlayStation Doom release date.
   time = (1 << 5) | (1 << 11);
   date = 16 | (11 << 5) | (15 << 9); 
   
   ob.WriteUint16(time); // file time (1:01)
   ob.WriteUint16(date); // file date (11/16/1995)

   if(file->len)
   {
      M_CRC32Initialize();
      file->crc = M_CRC32HashData(file->data, file->len);
   }
   else
      file->crc = 0;

   if(file->deflate)
   {
      auto tmpSize = compressBound(file->len);
      
      buffer.alloc(tmpSize, true);      
      auto tmpData = buffer.getAs<byte *>();

      auto res = Zip_Compress(tmpData, &tmpSize, file->data, file->len);
      if(res != Z_OK)
         I_Error("ZIP_WriteFile: compress returned error code %d\n", res);

      // write back compressed size
      file->clen = (uint32_t)tmpSize;
      data = tmpData;
      len  = file->clen;
   }
   else
   {
      data = file->data;
      len  = file->len;
   }

   ob.WriteUint32(file->crc);  // CRC-32
   ob.WriteUint32(file->clen); // compressed size
   ob.WriteUint32(file->len);  // uncompressed size

   namelen = (uint16_t)strlen(file->name);
   ob.WriteUint16(namelen);    // filename length
   ob.WriteUint16(0);          // extra field length

   // write file name
   ob.Write(file->name, namelen);

   // write file contents (stored or deflated)
   if(len)
      ob.Write(data, len);
}

//
// Zip_WriteDirEntry
//
// Write the central directory entry for a single file in the zip archive.
//
static void Zip_WriteDirEntry(ziparchive_t *zip, zipfile_t *file, OutBuffer &ob)
{
   unsigned short time, date;
   unsigned short namelen;

   ob.WriteUint32(0x02014b50); // central file header signature
   ob.WriteUint16(0x0B14);     // version made by 
   ob.WriteUint16(0x14);       // version needed to extract (2.0)

   // general purpose bit flag and compression method
   if(file->deflate)
   {
      ob.WriteUint16(2); // Max deflate compressed
      ob.WriteUint16(8); // compression method == deflate
   }
   else
   {
      ob.WriteUint16(0); // Nothing special    
      ob.WriteUint16(0); // compression method == store
   }

   // Time is psxwadgen version #; date is PSX Doom release date.
   time = (1 << 5) | (1 << 11);
   date = 16 | (11 << 5) | (15 << 9); 
   
   ob.WriteUint16(time);       // file time
   ob.WriteUint16(date);       // file date
   ob.WriteUint32(file->crc);  // CRC-32 (already calculated)
   ob.WriteUint32(file->clen); // compressed size
   ob.WriteUint32(file->len);  // uncompressed size

   namelen = (uint16_t)strlen(file->name);
   ob.WriteUint16(namelen);     // filename length
   ob.WriteUint16(0);           // extra field length
   ob.WriteUint16(0);           // file comment length
   ob.WriteUint16(0);           // disk number start
   
   ob.WriteUint16(file->intattr); // internal attributes
   ob.WriteUint32(file->extattr); // external attributes
   ob.WriteUint32(file->offset);  // local header offset

   // write the file name
   ob.Write(file->name, namelen);

   zip->dirlen += (46 + namelen);
}

//
// Zip_WriteEndOfDir
//
// Write the end of central directory structure.
//
static void Zip_WriteEndOfDir(ziparchive_t *zip, OutBuffer &ob)
{
   ob.WriteUint32(0x06054b50);     // end of central dir signature
   ob.WriteUint16(0);              // number of disk
   ob.WriteUint16(0);              // no. of disk with central dir
   ob.WriteUint16(zip->fcount);    // no. of central dir entries on disk
   ob.WriteUint16(zip->fcount);    // no. of central dir entries total
   ob.WriteUint32(zip->dirlen);    // size of central directory
   ob.WriteUint32(zip->diroffset); // offset of central dir
   ob.WriteUint16(0);              // length of zip comment
}

//
// Zip_Write
//
// Output the zip archive to disk.
//
void Zip_Write(ziparchive_t *zip)
{
   OutBuffer ob;
   zipfile_t *curfile;

   ob.CreateFile(zip->filename, 16384, OutBuffer::LENDIAN);

   // write files
   curfile = zip->files;

   while(curfile)
   {
      V_ProgressSpinner();
      Zip_WriteFile(curfile, ob);
      curfile = curfile->next;
   }

   ob.Flush();

   zip->diroffset = ob.Tell();

   // write central directory
   curfile = zip->files;

   while(curfile)
   {
      V_ProgressSpinner();
      Zip_WriteDirEntry(zip, curfile, ob);
      curfile = curfile->next;
   }

   // write end of central directory
   Zip_WriteEndOfDir(zip, ob);

   // close the file
   ob.Close();
}

#ifndef NO_UNIT_TESTS
//
// Zip_UnitTest
//
// Test function to try out the zip writer.
//
void Zip_UnitTest()
{
   ziparchive_t zip;
   zipfile_t *foo;
   zipfile_t *bar_folder;
   zipfile_t *baz;

   const char   *foodata = 
      "Hello Zip World!\r\n"
      "This is a test of the ability to store deflated text in a zip file.\r\n"
      "You should not see this text in the file, and it should be smaller than\r\n"
      "the length of this string by a bit.\r\n"
      "Goodbye!";
   unsigned int  bazdata = 0xDEADBEEF;

   Zip_Create(&zip, "test.zip");

   // add some files

   // a text file
   foo = Zip_AddFile(&zip, "foo.txt", (const byte *)foodata, strlen(foodata), ZIP_FILE_BINARY, true);
   
   // a folder
   bar_folder = Zip_AddFile(&zip, "bar/", NULL, 0, ZIP_DIRECTORY, false);
   
   // a binary file
   baz = Zip_AddFile(&zip, "bar/baz", (const byte *)&bazdata, sizeof(unsigned int), ZIP_FILE_BINARY, false);

   Zip_Write(&zip);
}
#endif

// EOF
