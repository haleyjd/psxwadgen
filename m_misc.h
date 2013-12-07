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
//    Misc stuff - config file, screenshots
//    
//-----------------------------------------------------------------------------

#ifndef M_MISC_H__
#define M_MISC_H__

#include "doomtype.h"

class qstring;

//
// MISC
//

bool  M_WriteFile(const char *name, void *source, size_t length);
int   M_ReadFile(const char *name, byte **buffer);
char *M_LoadStringFromFile(const char *filename);

// haleyjd: Portable versions of common non-standard C functions, as well as
// some misc string routines that really don't fit anywhere else. Some of these
// default to the platform implementation if its existence is verifiable 
// (see d_keywds.h)

char *M_Strupr(char *string);
char *M_Strlwr(char *string);
char *M_Itoa(int value, char *string, int radix);
int   M_CountNumLines(const char *str);

// Misc file routines
// haleyjd: moved a number of these here from w_wad module.

void  M_GetFilePath(const char *fn, char *base, size_t len); // haleyjd
long  M_FileLength(FILE *f);
void  M_ExtractFileBase(const char *, char *);               // killough
char *M_AddDefaultExtension(char *, const char *);           // killough 1/18/98
void  M_NormalizeSlashes(char *);                            // killough 11/98

int   M_StringAlloca(char **str, int numstrs, size_t extra, const char *str1, ...);
char *M_SafeFilePath(const char *pbasepath, const char *newcomponent);

bool M_FindCanonicalForm(const qstring &indir, const char *fn, qstring &out);

#endif

// EOF

