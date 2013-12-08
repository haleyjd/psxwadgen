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
#include "s_sfxgen.h"
#include "v_psx.h"
#include "w_formats.h"
#include "zip_write.h"

// Defines

// Default output file names for supported target formats
#define DEF_OUTPUTNAME_WAD "psxdoom.wad"
#define DEF_OUTPUTNAME_ZIP "psxdoom.pke"

// Globals

// Output format
WResourceFmt gOutputFormat = W_FORMAT_ZIP;

// Output targets
ziparchive_t gZipArchive; // zip file (ie. pke archive)

// Statics

static qstring baseinputdir; // root input directory; ex: J:\PSXDOOM
static qstring outputname;   // name of output file

//
// D_ExtractMovie
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

static const char *usagestr =
"\n"
"psxwadgen options:\n"
"\n"
"-input <dirpath> [-output <filename>]\n"
"  Generates a psxdoom.wad or psxdoom.pke output file, given the path to the\n"
"  PSXDOOM directory on a disc or mounted disc image of PlayStation DOOM.\n"
"\n"
"-movie <imgfile> [-output <filename>] [-start <secnum>] [-length <seclen>]\n"
"  Extracts the MOVIE.STR file from a raw CD image. Default output file name\n"
"  is movie.str; default sector start position is 822 and length is 1377,\n"
"  suitable for use with a North American release image.\n";

//
// D_PrintUsage
//
// Output documentation on all supported command-line parameters and exit.
//
static void D_PrintUsage()
{
   puts(usagestr);
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
   static const char *helpParams[] = { "-help", "-?" };

   // check for help
   if(M_CheckMultiParm(helpParams, 0))
      D_PrintUsage();

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

   // TODO: look for output format parameter

   // set default output filename based on selected output format
   if(gOutputFormat == W_FORMAT_ZIP)
      outputname = DEF_OUTPUTNAME_ZIP;

   // allow command-line override
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
// Output Management
//

//
// D_OpenOutputFile
//
// Create the output structure that will be passed to other modules to 
// accumulate data entries (ie., lumps).
//
static void D_OpenOutputFile()
{
   switch(gOutputFormat)
   {
   case W_FORMAT_ZIP:
      Zip_Create(&gZipArchive, outputname.constPtr());
      break;
   default:
      I_Error("D_OpenOutputFile: unsupported output format value %d\n", 
              gOutputFormat);
      break;
   }

   printf("D_OpenOutputFile: output directed to %s\n", outputname.constPtr());
}

//
// D_TransformToZip
//
// Perform all steps necessary to create the psxdoom.pke archive.
//
static void D_TransformToZip()
{
   // sounds
   S_ProcessSoundsForZip(baseinputdir, &gZipArchive);

   // textures
   V_ConvertTexturesToZip(psxIWAD, &gZipArchive);

   // flats
   V_ConvertFlatsToZip(psxIWAD, &gZipArchive);
}

//
// D_TransformInput
//
// Call the proper routine to transform the input to the output format.
//
static void D_TransformInput()
{
   switch(gOutputFormat)
   {
   case W_FORMAT_ZIP:
      D_TransformToZip();
      break;
   default:
      break;
   }
}

//
// D_CloseOutputFile
//
// Write and close the output file once everything has been added to it.
//
static void D_CloseOutputFile()
{
   switch(gOutputFormat)
   {
   case W_FORMAT_ZIP:
      Zip_Write(&gZipArchive);
      break;
   default:
      break;
   }
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

   // open output
   D_OpenOutputFile();

   // transform
   D_TransformInput();

   // close output
   D_CloseOutputFile();

   return 0;
}

// EOF

