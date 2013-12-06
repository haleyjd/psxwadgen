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
//   PSX Sound Lookup
//
//-----------------------------------------------------------------------------

#ifndef S_SOUNDS_H__
#define S_SOUNDS_H__

// This is an enum of sounds we want to create, and an index into the info
// table. These are NOT indices into the LCD files, which are in the actual
// info table. This is done so that certain sounds may share the same LCD
// entry, and therefore get multiple lumps created for them in the output 
// archive.
enum psxsfxnum_e
{
   sfx_pistol,
   sfx_shotgn,
   sfx_sgcock,
   sfx_wpnup,   // duplicate of sgcock
   sfx_plasma,
   sfx_bfg,
   sfx_sawup,
   sfx_sawidl,
   sfx_sawful,
   sfx_sawhit,
   sfx_rlaunc,
   sfx_itembk,
   sfx_rxplod,
   sfx_firsht,
   sfx_firxpl,
   sfx_pstart,
   sfx_pstop,
   sfx_doropn,
   sfx_dorcls,
   sfx_stnmov,
   sfx_swtchn,
   sfx_swtchx,
   sfx_plpain,
   sfx_dmpain,
   sfx_popain,
   sfx_slop,
   sfx_itemup,
   sfx_oof,
   sfx_noway,  // copy of oof
   sfx_skldth, // copy of oof
   sfx_telept,
   sfx_posit1,
   sfx_posit2,
   sfx_posit3,
   sfx_bgsit1,
   sfx_bgsit2,
   sfx_sgtsit,
   sfx_cacsit,
   sfx_brssit,
   sfx_cybsit,
   sfx_spisit,
   sfx_sklatk,
   sfx_sgtatk,
   sfx_claw,
   sfx_pldeth,
   sfx_pdiehi, // copy of pldeth
   sfx_podth1,
   sfx_podth2,
   sfx_podth3,
   sfx_bgdth1,
   sfx_bgdth2,
   sfx_sgtdth,
   sfx_cacdth,
   sfx_flamst,
   sfx_brsdth,
   sfx_cybdth,
   sfx_spidth,
   sfx_posact,
   sfx_bgact,
   sfx_dmact,
   sfx_barexp,
   sfx_punch,
   sfx_hoof,
   sfx_metal,
   sfx_dshtgn,
   sfx_dbopn,
   sfx_dbload,
   sfx_dbcls,
   sfx_kntsit,
   sfx_kntdth,
   sfx_pesit,
   sfx_pepain,
   sfx_pedth,
   sfx_bspsit,
   sfx_bspdth,
   sfx_bspact,
   sfx_bspwlk,
   sfx_manatk,
   sfx_mansit,
   sfx_mnpain,
   sfx_mandth,
   sfx_skesit,
   sfx_skedth,
   sfx_skeact,
   sfx_skeatk,
   sfx_skeswg,
   sfx_skepch,
   sfx_bdopn,
   sfx_bdcls,
   sfx_getpow,
   NUMPSXSFX
};

struct psxsfxinfo_t
{
   const char *name;  // base name to use in archives
   int         sfxID; // ID number in LCD files (NOT enum value)
};

extern psxsfxinfo_t psxsfxinfo[NUMPSXSFX];

#endif

// EOF

