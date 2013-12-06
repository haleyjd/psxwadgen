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
#include "w_formats.h"

// Globals

// Output format
WResourceFmt gOutputFormat = W_FORMAT_ZIP;

static qstring baseinputdir;
static qstring outputname;

//
// DoExtractMovie
//
// Mini-program to extract the MOVIE.STR file
//
static void D_ExtractMovie()
{
   qstring infile, outfile;
   int p, start = 0, length = 0;

   // input file name - required
   if((p = M_CheckParm("-movie")) && p < myargc - 1)
      infile = myargv[p+1];
   else
      I_Error("D_ExtractMovie: need a raw CD image file for input\n");

   // output file name - optional
   if((p = M_CheckParm("-output")) && p < myargc - 1)
      outfile = myargv[p+1];
   else
      outfile = "movie.str";

   // start and length sector overrides - optional
   if((p = M_CheckParm("-start")) && p < myargc - 1)
      start = atoi(myargv[p+1]);
   if((p = M_CheckParm("-length")) && p < myargc - 1)
      length = atoi(myargv[p+1]);

   V_ExtractMovie(infile, outfile, start, length);
   exit(0);
}

//
// CheckForParameters
//
// Check for command line parameters
//
static void D_CheckForParameters()
{
   int p;

   // check for movie file extraction
   if(M_CheckParm("-movie"))
   {
      printf("D_ExtractMovie: extracting MOVIE.STR from CD image\n");
      D_ExtractMovie();
   }

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
   printf("D_CheckForParameters: Checking command line parameters.\n");
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

