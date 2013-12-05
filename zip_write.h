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

#ifndef ZIP_WRITE_H__
#define ZIP_WRITE_H__

#include "doomtype.h"

//
// zipfile - a single file to be added to the zip
//
struct zipfile_t
{
   zipfile_t  *next;    // next file

   const char *name;    // file name
   const byte *data;    // file data
   uint32_t    len;     // length of data
   uint32_t    clen;    // length of data when compressed (== len if not)
   long        offset;  // offset in physical file after written
   uint32_t    crc;     // cached CRC value
   uint32_t    extattr; // external attributes
   uint16_t    intattr; // internal attributes
   bool        deflate; // if true, use deflate compression
};

//
// ziparchive - represents the entire zip file
//
struct ziparchive_t
{
   zipfile_t  *files;     // list of files
   zipfile_t  *last;      // last file
   const char *filename;  // physical file name
   long        diroffset; // offset of central directory
   uint16_t    fcount;    // count of files
   uint32_t    dirlen;    // length of central directory
};

// Zip File Types
enum ziptype_e
{
   ZIP_FILE_BINARY,
   ZIP_FILE_TEXT,
   ZIP_DIRECTORY
};

// Initialize a zip archive structure.
void Zip_Create(ziparchive_t *zip, const char *filename);

// Add a new file or directory entry to a zip archive.
// zip      - an initialized ziparchive structure
// name     - name of the file within the archive, including any subdirectories
// data     - source of data for the file, if any (NULL for directories)
// len      - amount of information pointed to by data in bytes; 0 if none.
// fileType - one of the ziptype_e enumeration values
// deflate  - if true, the file will be compressed using zlib deflate;
//            otherwise, "store" method will be used (direct copy)
// Returns: A new zipfile_t structure
zipfile_t *Zip_AddFile(ziparchive_t *zip, const char *name, const byte *data, 
                       uint32_t len, ziptype_e fileType, bool deflate);

// Call to write the zip archive to disk.
void Zip_Write(ziparchive_t *zip);

// Unit test function
void Zip_UnitTest();

#endif

// EOF

