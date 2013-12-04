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

#include "m_buffer.h"
#include "zip_write.h"

//=============================================================================
//
// Zip Functions
//

//
// Zip_Create
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
zipfile_t *Zip_AddFile(ziparchive_t *zip, const char *name, const byte *data, 
                       unsigned int len, ziptype_e fileType)
{
   auto file = estructalloc(zipfile_t, 1);

   file->name = name;
   file->data = data;
   file->len  = len;

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
// Zip_WriteFile
//
void Zip_WriteFile(zipfile_t *file, OutBuffer &ob)
{
   uint16_t date, time;
   uint16_t namelen;

   ob.Flush();

   file->offset = ob.Tell();

   ob.WriteUint32(0x04034b50); // local file header signature
   ob.WriteUint16(0x0A);       // version needed to extract (1.0)
   ob.WriteUint16(0);          // general purpose bit flag
   ob.WriteUint16(0);          // compression method == store
   
   // Time is psxwadgen version #; date is PlayStation Doom release date.
   time = (1 << 5) | (1 << 11);
   date = 11 | (16 << 5) | (15 << 9);
   
   ob.WriteUint16(time);       // file time
   ob.WriteUint16(date);       // file date (11/16/1995)

   if(file->len)
   {
      M_CRC32Initialize();
      file->crc = M_CRC32HashData(file->data, file->len);
   }
   else
      file->crc = 0;

   ob.WriteUint32(file->crc);  // CRC-32
   ob.WriteUint32(file->len);  // compressed size
   ob.WriteUint32(file->len);  // uncompressed size

   namelen = (uint16_t)strlen(file->name);
   ob.WriteUint16(namelen);    // filename length
   ob.WriteUint16(0);          // extra field length

   // write file name
   ob.Write(file->name, namelen);

   // write file contents
   if(file->len)
      ob.Write(file->data, file->len);
}

//
// Zip_WriteDirEntry
//
void Zip_WriteDirEntry(ziparchive_t *zip, zipfile_t *file, OutBuffer &ob)
{
   unsigned short time, date;
   unsigned short namelen;

   ob.WriteUint32(0x02014b50); // central file header signature
   ob.WriteUint16(0x0B14);     // version made by (Info-Zip NTFS)
   ob.WriteUint16(0x0A);       // version needed to extract (1.0)
   ob.WriteUint16(0);          // general purpose bit flag
   ob.WriteUint16(0);          // compression method == store

   // Time is psxwadgen version #; date is PSX Doom release date.
   time = (1 << 5) | (1 << 11);
   date = 11 | (16 << 5) | (15 << 9);
   
   ob.WriteUint16(time);       // file time (3:35)
   ob.WriteUint16(date);       // file date (3/5/2093)
   ob.WriteUint32(file->crc);  // CRC-32 (already calculated)
   ob.WriteUint32(file->len);  // compressed size
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
void Zip_WriteEndOfDir(ziparchive_t *zip, OutBuffer &ob)
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
void Zip_Write(ziparchive_t *zip)
{
   OutBuffer ob;
   zipfile_t *curfile;

   ob.CreateFile(zip->filename, 16384, OutBuffer::LENDIAN);

   // write files
   curfile = zip->files;

   while(curfile)
   {
      Zip_WriteFile(curfile, ob);
      curfile = curfile->next;
   }

   ob.Flush();

   zip->diroffset = ob.Tell();

   // write central directory
   curfile = zip->files;

   while(curfile)
   {
      Zip_WriteDirEntry(zip, curfile, ob);
      curfile = curfile->next;
   }

   // write end of central directory
   Zip_WriteEndOfDir(zip, ob);

   // close the file
   ob.Close();
}

#if 0
//
// main
//
int main(void)
{
   ziparchive_t zip;
   zipfile_t *foo;
   zipfile_t *bar_folder;
   zipfile_t *baz;

   const char   *foodata = "Hello Zip World!\n";
   unsigned int  bazdata = 0xDEADBEEF;

   Zip_Create(&zip, "test.zip");

   // add some files
   foo        = Zip_AddFile(&zip, "foo.txt", foodata,          strlen(foodata));
   bar_folder = Zip_AddFile(&zip, "bar/",    NULL,             0);
   baz        = Zip_AddFile(&zip, "bar/baz", (byte *)&bazdata, sizeof(unsigned int));

   // set attributes
   foo->intattr        = 0x01; // is a text file
   foo->extattr        = 0x20; // set archive flag
   bar_folder->extattr = 0x10; // set directory flag
   baz->extattr        = 0x20; // set archive flag

   Zip_Write(&zip);

   return 0;
}
#endif

// EOF
