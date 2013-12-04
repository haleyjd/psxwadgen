// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2002 James Haley
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
// DeHackEd information tables / hashing
//
// Separated out from d_deh.c to avoid clutter and speed up the
// editing of that file. Routines for internal table hash chaining
// and initialization are also here.
//
//--------------------------------------------------------------------------

#include "z_zone.h"
#include "i_system.h"

#include "d_dehtbl.h"
#include "d_io.h"
#include "doomtype.h"
#include "m_argv.h"
#include "m_fixed.h"

//
// Shared Hash functions
//

//
// D_HashTableKey
//
// Fairly standard key computation -- this is used for multiple 
// tables so there's not much use trying to make it perfect. It'll 
// save time anyways.
// 08/28/03: vastly simplified, is now similar to SGI's STL hash
// 08/23/13: rewritten to use sdbm hash algorithm
//
unsigned int D_HashTableKey(const char *str)
{
   auto ustr = reinterpret_cast<const unsigned char *>(str);
   int c;
   unsigned int h = 0;

#ifdef RANGECHECK
   if(!ustr)
      I_Error("D_HashTableKey: cannot hash NULL string!\n");
#endif

   // note: this needs to be case insensitive for EDF mnemonics
   while((c = *ustr++))
      h = ectype::toUpper(c) + (h << 6) + (h << 16) - h;

   return h;
}

//
// D_HashTableKeyCase
//
// haleyjd 09/09/09: as above, but case-sensitive
//
unsigned int D_HashTableKeyCase(const char *str)
{
   auto ustr = reinterpret_cast<const unsigned char *>(str);
   int c;
   unsigned int h = 0;

#ifdef RANGECHECK
   if(!ustr)
      I_Error("D_HashTableKeyCase: cannot hash NULL string!\n");
#endif

   while((c = *ustr++))
      h = c + (h << 6) + (h << 16) - h;

   return h;
}

// EOF


