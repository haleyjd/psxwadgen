// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2000 James Haley
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
//--------------------------------------------------------------------------
//
// DESCRIPTION:
//  Default Config File.
//
// NETCODE_FIXME -- CONSOLE_FIXME -- CONFIG_FIXME:
// All configuration file code is redundant with the console variable
// system. These systems sorely need to be integrated so that configuration
// comes from a console script instead of a separate file format. Need
// to do this without breaking config-in-wad capability, or need to 
// come up with some other easy way for projects to override defaults.
// ALL archived console variables need to be adjusted to have defaults,
// and a way to set their value without changing the default must be
// added. See other CONSOLE_FIXME notes for more info.
//
//-----------------------------------------------------------------------------

#include "z_zone.h"

#include "d_dehtbl.h"
#include "d_io.h"
#include "i_opndir.h"
#include "i_system.h"
#include "m_argv.h"
#include "m_misc.h"
#include "m_qstr.h"
#include "w_wad.h"

//=============================================================================
//
// File IO Routines
//

//
// M_WriteFile
//
// killough 9/98: rewritten to use stdio and to flash disk icon
//
bool M_WriteFile(char const *name, void *source, size_t length)
{
   FILE *fp;
   bool result;
   
   errno = 0;
   
   if(!(fp = fopen(name, "wb")))         // Try opening file
      return 0;                          // Could not open file for writing
   
   //I_BeginRead();                       // Disk icon on
   result = (fwrite(source, 1, length, fp) == length);   // Write data
   fclose(fp);
   //I_EndRead();                         // Disk icon off
   
   if(!result)                          // Remove partially written file
      remove(name);
   
   return result;
}

//
// M_ReadFile
//
// killough 9/98: rewritten to use stdio and to flash disk icon
//
int M_ReadFile(char const *name, byte **buffer)
{
   FILE *fp;
   
   errno = 0;
   
   if((fp = fopen(name, "rb")))
   {
      size_t length;

      //I_BeginRead();
      fseek(fp, 0, SEEK_END);
      length = ftell(fp);
      fseek(fp, 0, SEEK_SET);

      *buffer = ecalloc(byte *, 1, length);
      
      if(fread(*buffer, 1, length, fp) == length)
      {
         fclose(fp);
         //I_EndRead();
         return length;
      }
      fclose(fp);
   }

   // sf: do not quit on file not found
   //  I_Error("Couldn't read file %s: %s", name, 
   //	  errno ? strerror(errno) : "(Unknown Error)");
   
   return -1;
}

// 
// M_FileLength
//
// Gets the length of a file given its handle.
// haleyjd 03/09/06: made global
// haleyjd 01/04/10: use fseek/ftell
//
long M_FileLength(FILE *f)
{
   long curpos, len;

   curpos = ftell(f);
   fseek(f, 0, SEEK_END);
   len = ftell(f);
   fseek(f, curpos, SEEK_SET);

   return len;
}

//
// M_LoadStringFromFile
//
// haleyjd 09/02/12: Like M_ReadFile, but assumes the contents are a string
// and therefore null terminates the buffer.
//
char *M_LoadStringFromFile(const char *filename)
{
   FILE  *f    = NULL;
   char  *buf  = NULL;
   size_t len = 0;
   
   if(!(f = fopen(filename, "rb")))
      return NULL;

   // allocate at length + 1 for null termination
   len = static_cast<size_t>(M_FileLength(f));
   buf = ecalloc(char *, 1, len + 1);
   if(fread(buf, 1, len, f) != len)
      printf("M_LoadStringFromFile: warning: could not read file %s\n", filename);
   fclose(f);

   return buf;
}

//=============================================================================
//
// Portable non-standard libc functions and misc string operations
//

// haleyjd: portable strupr function
char *M_Strupr(char *string)
{
   char *s = string;

   while(*s)
   {
      int c = ectype::toUpper(*s);
      *s++ = c;
   }

   return string;
}

// haleyjd: portable strlwr function
char *M_Strlwr(char *string)
{
   char *s = string;

   while(*s)
   {
      int c = ectype::toLower(*s);
      *s++ = c;
   }

   return string;
}

// haleyjd: portable itoa, from DJGPP libc

/* Copyright (C) 2001 DJ Delorie, see COPYING.DJ for details */

char *M_Itoa(int value, char *string, int radix)
{
#ifdef EE_HAVE_ITOA
   return ITOA_NAME(value, string, radix);
#else
   char tmp[33];
   char *tp = tmp;
   int i;
   unsigned int v;
   int sign;
   char *sp;

   if(radix > 36 || radix <= 1)
   {
      errno = EDOM;
      return 0;
   }

   sign = (radix == 10 && value < 0);

   if(sign)
      v = -value;
   else
      v = (unsigned int)value;

   while(v || tp == tmp)
   {
      i = v % radix;
      v = v / radix;

      if(i < 10)
         *tp++ = i + '0';
      else
         *tp++ = i + 'a' - 10;
   }

   if(string == 0)
      string = (char *)(malloc)((tp-tmp)+sign+1);
   sp = string;

   if(sign)
      *sp++ = '-';

   while(tp > tmp)
      *sp++ = *--tp;
   *sp = 0;

   return string;
#endif
}

//
// M_CountNumLines
//
// Counts the number of lines in a string. If the string length is greater than
// 0, we consider the string to have at least one line.
//
int M_CountNumLines(const char *str)
{
   const char *rover = str;
   int numlines = 0;
   char c;

   if(strlen(str))
   {
      numlines = 1;

      while((c = *rover++))
      {
         if(c == '\n')
            ++numlines;
      }
   }

   return numlines;
}

//=============================================================================
//
// Filename and Path Routines
//

//
// M_GetFilePath
//
// haleyjd: General file path extraction. Strips a path+filename down to only
// the path component.
//
void M_GetFilePath(const char *fn, char *base, size_t len)
{
   bool found_slash = false;
   char *p;

   memset(base, 0, len);

   p = base + len - 1;

   strncpy(base, fn, len);
   
   while(p >= base)
   {
      if(*p == '/' || *p == '\\')
      {
         found_slash = true; // mark that the path ended with a slash
         *p = '\0';
         break;
      }
      *p = '\0';
      p--;
   }

   // haleyjd: in the case that no slash was ever found, yet the
   // path string is empty, we are dealing with a file local to the
   // working directory. The proper path to return for such a string is
   // not "", but ".", since the format strings add a slash now. When
   // the string is empty but a slash WAS found, we really do want to
   // return the empty string, since the path is relative to the root.
   if(!found_slash && *base == '\0')
      *base = '.';
}

//
// M_ExtractFileBase
//
// Extract an up-to-eight-character filename out of a full file path
// (relative or absolute), removing the path components and extensions.
// This is not a general filename extraction routine and should only
// be used for generating WAD lump names from files.
//
// haleyjd 04/17/11: Added support for truncation of LFNs courtesy of
// Choco Doom. Thanks, fraggle ;)
//
void M_ExtractFileBase(const char *path, char *dest)
{
   const char *src = path + strlen(path) - 1;
   int length;
   
   // back up until a \ or the start
   while(src != path && src[-1] != ':' // killough 3/22/98: allow c:filename
         && *(src-1) != '\\'
         && *(src-1) != '/')
   {
      src--;
   }

   // copy up to eight characters
   // FIXME: insecure, does not ensure null termination of output string!
   memset(dest, 0, 8);
   length = 0;

   while(*src && *src != '.')
   {
      if(length >= 8)
         break; // stop at 8

      dest[length++] = ectype::toUpper(*src++);
   }
}

//
// M_AddDefaultExtension
//
// 1/18/98 killough: adds a default extension to a path
// Note: Backslashes are treated specially, for MS-DOS.
//
// Warning: the string passed here *MUST* have room for an
// extension to be added to it! -haleyjd
//
char *M_AddDefaultExtension(char *path, const char *ext)
{
   char *p = path;
   while(*p++);
   while(p-- > path && *p != '/' && *p != '\\')
      if(*p == '.')
         return path;
   if(*ext != '.')
      strcat(path, ".");
   return strcat(path, ext);
}

//
// M_NormalizeSlashes
//
// Remove trailing slashes, translate backslashes to slashes
// The string to normalize is passed and returned in str
//
// killough 11/98: rewritten
//
void M_NormalizeSlashes(char *str)
{
   char *p;
   char useSlash      = '/'; // The slash type to use for normalization.
   char replaceSlash = '\\'; // The kind of slash to replace.
   bool isUNC = false;

   //if(ee_current_platform == EE_PLATFORM_WINDOWS)
   {
      // This is an UNC path; it should use backslashes.
      // NB: We check for both in the event one was changed earlier by mistake.
      if(strlen(str) > 2 && 
         ((str[0] == '\\' || str[0] == '/') && str[0] == str[1]))
      {
         useSlash = '\\';
         replaceSlash = '/';
         isUNC = true;
      }
   }
   
   // Convert all replaceSlashes to useSlashes
   for(p = str; *p; p++)
   {
      if(*p == replaceSlash)
         *p = useSlash;
   }

   // Remove trailing slashes
   while(p > str && *--p == useSlash)
      *p = 0;

   // Collapse multiple slashes
   for(p = str + (isUNC ? 2 : 0); (*str++ = *p); )
      if(*p++ == useSlash)
         while(*p == useSlash)
            p++;
}

//
// M_StringAlloca
//
// haleyjd: This routine takes any number of strings and a number of extra
// characters, calculates their combined length, and calls Z_Alloca to create
// a temporary buffer of that size. This is extremely useful for allocation of
// file paths, and is used extensively in d_main.c.  The pointer returned is
// to a temporary Z_Alloca buffer, which lives until the next main loop
// iteration, so don't cache it. Note that this idiom is not possible with the
// normal non-standard alloca function, which allocates stack space.
//
int M_StringAlloca(char **str, int numstrs, size_t extra, const char *str1, ...)
{
   va_list args;
   size_t len = extra;

   if(numstrs < 1)
      I_Error("M_StringAlloca: invalid input\n");

   len += strlen(str1);

   --numstrs;

   if(numstrs != 0)
   {   
      va_start(args, str1);
      
      while(numstrs != 0)
      {
         const char *argstr = va_arg(args, const char *);
         
         len += strlen(argstr);
         
         --numstrs;
      }
      
      va_end(args);
   }

   ++len;

   *str = (char *)(Z_Alloca(len));

   return len;
}

//
// M_SafeFilePath
//
// haleyjd 09/10/11: back-adapted from Chocolate Strife to provide secure 
// file path concatenation with automatic normalization on alloca-provided 
// buffers.
//
char *M_SafeFilePath(const char *pbasepath, const char *newcomponent)
{
   int     newstrlen = 0;
   char   *newstr    = NULL;

   newstrlen = M_StringAlloca(&newstr, 3, 1, pbasepath, "/", newcomponent);
   psnprintf(newstr, newstrlen, "%s/%s", pbasepath, newcomponent);
   M_NormalizeSlashes(newstr);

   return newstr;
}

//
// M_FindCanonicalForm
//
// Given an input directory and a desired file name, find the canoncial form
// of that file name regardless of case and return it in "out".
// Returns true if the file was found and false otherwise.
//
bool M_FindCanonicalForm(const qstring &indir, const char *fn, qstring &out)
{
   bool    res = false;
   DIR    *dir;
   dirent *ent;
   
   if(!(dir = opendir(indir.constPtr())))
      return false;

   while((ent = readdir(dir)))
   {
      if(!strcasecmp(fn, ent->d_name))
      {
         out = ent->d_name;
         res = true;
         break;
      }
   }

   closedir(dir);

   return res;
}

// EOF

