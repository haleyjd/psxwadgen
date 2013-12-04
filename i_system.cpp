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

#include "z_zone.h"
#include "i_system.h"

static int error_exitcode;
static bool has_exited;

enum
{
   I_ERRORLEVEL_NONE,    // no error
   I_ERRORLEVEL_MESSAGE, // not really an error, just an exit message
   I_ERRORLEVEL_NORMAL,  // a "normal" error (such as a missing patch)
   I_ERRORLEVEL_FATAL    // kill with a vengeance
};

//
// I_FatalError
//
// haleyjd 05/21/10: Call this for super-evil errors such as heap corruption,
// system-related problems, etc.
//
void I_FatalError(int code, const char *error, ...)
{
   // Flag a fatal error, so that some shutdown code will not be executed;
   // chiefly, saving the configuration files, which can malfunction in
   // unpredictable ways when heap corruption is present. We do this even
   // if an error has already occurred, since, for example, if a Z_ChangeTag
   // error happens during M_SaveDefaults, we do not want to subsequently
   // run M_SaveSysConfig etc. in I_Quit.
   error_exitcode = I_ERRORLEVEL_FATAL;

   if(code == I_ERR_ABORT)
   {
      // kill with utmost contempt
      abort();
   }
   else
   {
      va_list argptr;
      va_start(argptr, error);
      vprintf(error, argptr);
      va_end(argptr);

      if(!has_exited)    // If it hasn't exited yet, exit now -- killough
      {
         has_exited = 1; // Prevent infinitely recursive exits -- killough
         exit(-1);
      }
      else
         abort(); // double fault, must abort
   }
}

//
// I_Error
//
// Normal error reporting / exit routine.
//
void I_Error(const char *error, ...) // killough 3/20/98: add const
{
   // do not demote error level
   if(error_exitcode < I_ERRORLEVEL_NORMAL)
      error_exitcode = I_ERRORLEVEL_NORMAL; // a normal error

   va_list argptr;
   va_start(argptr,error);
   vprintf(error, argptr);
   va_end(argptr);
   
   if(!has_exited)    // If it hasn't exited yet, exit now -- killough
   {
      has_exited = 1; // Prevent infinitely recursive exits -- killough
      exit(-1);
   }
   else
      I_FatalError(I_ERR_ABORT, "I_Error: double faulted\n");
}

//
// I_ErrorVA
//
// haleyjd: varargs version of I_Error used chiefly by libConfuse.
//
void I_ErrorVA(const char *error, va_list args)
{
   // do not demote error level
   if(error_exitcode < I_ERRORLEVEL_NORMAL)
      error_exitcode = I_ERRORLEVEL_NORMAL;

   va_list argptr;
   va_start(argptr,error);
   vprintf(error, args);
   va_end(argptr);

   if(!has_exited)
   {
      has_exited = 1;
      exit(-1);
   }
   else
      I_FatalError(I_ERR_ABORT, "I_ErrorVA: double faulted\n");
}

// EOF

