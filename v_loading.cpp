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
//   Loading progress bar
//
//-----------------------------------------------------------------------------

#include "z_zone.h"

static int loading_total;
static int loading_amount;

//
// V_SetLoading
//
void V_SetLoading(int total, bool newline)
{
   loading_total = total;
   loading_amount = 0;

   if(newline)
      printf("\n ");

   putchar('[');
   for(int i = 0; i < total; i++) 
      putchar(' ');     // gap
   putchar(']');
   for(int i = 0; i <= total; i++) 
      putchar('\b');    // backspace
}

//
// V_LoadingIncrease
//
void V_LoadingIncrease()
{
   if(loading_amount < loading_total)
   {
      loading_amount++;
      putchar('.');
      if(loading_amount == loading_total) 
         putchar('\n');
   }
}

//
// V_ProgressSpinner
//
// From BSP 5.2; used under GPLv2.
//
void V_ProgressSpinner()
{
   static unsigned int pcnt;

   if(!((++pcnt)&31))
      printf("%c\b","/-\\|"[((pcnt)>>5)&3]);
}


// EOF

