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
//   Main module
//
//-----------------------------------------------------------------------------

#include "z_zone.h"

#include "d_wads.h"
#include "i_system.h"
#include "m_argv.h"
#include "m_qstr.h"
#include "v_psx.h"

static qstring baseinputdir;
static qstring outputname("psxdoom.wad");

//
// CheckForParameters
//
// Check for command line parameters
//
static void D_CheckForParameters()
{
   int p;

   printf("D_CheckForParameters: Checking command line parameters\n");

   if((p = M_CheckParm("-input")) && p < myargc - 1)
      baseinputdir = myargv[p+1];
   else
      I_Error("Need a directory name for input!\n");

   if((p = M_CheckParm("-output")) && p < myargc - 1)
      outputname = myargv[p+1];
}

//
// Startup Banner
//
static void D_PrintStartupBanner()
{
   puts("psxwadgen\n"
        "Copyright 2013 James Haley et al.\n"
        "This program is free software distributed under the terms of the GNU General\n"
        "Public License. See the file \"COPYING\" for full details.\n");
}

//
// Initialization
//
static void D_Init()
{
   D_PrintStartupBanner();

   // Zone init
   printf("Z_Init: Init zone allocation daemon.\n");
   Z_Init();

   // Check for command line parameters
   D_CheckForParameters();

   // Load PSX wad files from input directory
   printf("D_LoadInputFiles: Loading PSX wad files...\n");
   D_LoadInputFiles(baseinputdir);
}

//
// Main Program
//
int main(int argc, char **argv)
{
   // setup m_argv
   myargc = argc;
   myargv = argv;

   // perform initialization
   D_Init();

   return 0;
}

// EOF

