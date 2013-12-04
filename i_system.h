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
//   System functions
//
//-----------------------------------------------------------------------------

#ifndef I_SYSTEM_H__
#define I_SYSTEM_H__

// haleyjd 05/21/10: error codes for I_FatalError
enum
{
   I_ERR_KILL,  // exit and do not perform shutdown actions
   I_ERR_ABORT  // call abort()
};

//SoM 3/14/2002: vc++ 
void I_Error(const char *error, ...);

void I_ErrorVA(const char *error, va_list args);

// haleyjd 05/21/10
void I_FatalError(int code, const char *error, ...);

#endif

// EOF

