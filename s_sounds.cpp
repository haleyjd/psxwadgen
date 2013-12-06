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

#include "s_sounds.h"

psxsfxinfo_t psxsfxinfo[NUMPSXSFX] =
{
   { "DSPISTOL",  0 }, // sfx_pistol
   { "DSSHOTGN",  1 }, // sfx_shotgn
   { "DSSGCOCK",  2 }, // sfx_sgcock
   { "DSWPNUP",   2 }, // sfx_wpnup   - copy of sgcock
   { "DSPLASMA",  3 }, // sfx_plasma
   { "DSBFG",     4 }, // sfx_bfg
   { "DSSAWUP",   5 }, // sfx_sawup
   { "DSSAWIDL",  6 }, // sfx_sawidl
   { "DSSAWFUL",  7 }, // sfx_sawful
   { "DSSAWHIT",  8 }, // sfx_sawhit
   { "DSRLAUNC",  9 }, // sfx_rlaunc
   { "DSITEMBK", 10 }, // sfx_itembk
   { "DSRXPLOD", 11 }, // sfx_rxplod
   { "DSFIRSHT", 12 }, // sfx_firsht
   { "DSFIRXPL", 13 }, // sfx_firxpl
   { "DSPSTART", 14 }, // sfx_pstart
   { "DSPSTOP",  15 }, // sfx_pstop
   { "DSDOROPN", 16 }, // sfx_doropn
   { "DSDORCLS", 17 }, // sfx_dorcls
   { "DSSTNMOV", 18 }, // sfx_stnmov
   { "DSSWTCHN", 19 }, // sfx_swtchn
   { "DSSWTCHX", 20 }, // sfx_swtchx
   { "DSPLPAIN", 21 }, // sfx_plpain
   { "DSDMPAIN", 22 }, // sfx_dmpain
   { "DSPOPAIN", 23 }, // sfx_popain
   { "DSSLOP",   24 }, // sfx_slop
   { "DSITEMUP", 25 }, // sfx_itemup
   { "DSOOF",    26 }, // sfx_oof
   { "DSNOWAY",  26 }, // sfx_noway  - copy of oof
   { "DSSKLDTH", 26 }, // sfx_skldth - copy of oof
   { "DSTELEPT", 27 }, // sfx_telept
   { "DSPOSIT1", 28 }, // sfx_posit1
   { "DSPOSIT2", 29 }, // sfx_posit2
   { "DSPOSIT3", 30 }, // sfx_posit3
   { "DSBGSIT1", 31 }, // sfx_bgsit1
   { "DSBGSIT2", 32 }, // sfx_bgsit2
   { "DSSGTSIT", 33 }, // sfx_sgtsit
   { "DSCACSIT", 34 }, // sfx_cacsit
   { "DSBRSSIT", 35 }, // sfx_brssit
   { "DSCYBSIT", 36 }, // sfx_cybsit
   { "DSSPISIT", 37 }, // sfx_spisit
   { "DSSKLATK", 38 }, // sfx_sklatk
   { "DSSGTATK", 39 }, // sfx_sgtatk
   { "DSCLAW",   40 }, // sfx_claw
   { "DSPLDETH", 41 }, // sfx_pldeth
   { "DSPDIEHI", 41 }, // sfx_pdiehi - copy of pldeth
   { "DSPODTH1", 42 }, // sfx_podth1
   { "DSPODTH2", 43 }, // sfx_podth2
   { "DSPODTH3", 44 }, // sfx_podth3
   { "DSBGDTH1", 45 }, // sfx_bgdth1
   { "DSBGDTH2", 46 }, // sfx_bgdth2
   { "DSSGTDTH", 47 }, // sfx_sgtdth
   { "DSCACDTH", 48 }, // sfx_cacdth
   { "DSFLAMST", 49 }, // sfx_flamst
   { "DSBRSDTH", 50 }, // sfx_brsdth
   { "DSCYBDTH", 51 }, // sfx_cybdth
   { "DSSPIDTH", 52 }, // sfx_spidth
   { "DSPOSACT", 53 }, // sfx_posact
   { "DSBGACT",  54 }, // sfx_bgact
   { "DSDMACT",  55 }, // sfx_dmact
   { "DSBAREXP", 56 }, // sfx_barexp
   { "DSPUNCH",  57 }, // sfx_punch
   { "DSHOOF",   58 }, // sfx_hoof
   { "DSMETAL",  59 }, // sfx_metal
   { "DSDSHTGN", 60 }, // sfx_dshtgn
   { "DSDBOPN",  61 }, // sfx_dbopn
   { "DSDBLOAD", 62 }, // sfx_dbload
   { "DSDBCLS",  63 }, // sfx_dbcls
   { "DSKNTSIT", 64 }, // sfx_kntsit
   { "DSKNTDTH", 65 }, // sfx_kntdth
   { "DSPESIT",  66 }, // sfx_pesit
   { "DSPEPAIN", 67 }, // sfx_pepain
   { "DSPEDTH",  68 }, // sfx_pedth
   { "DSBSPSIT", 69 }, // sfx_bspsit
   { "DSBSPDTH", 70 }, // sfx_bspdth
   { "DSBSPACT", 71 }, // sfx_bspact
   { "DSBSPWLK", 72 }, // sfx_bspwlk
   { "DSMANATK", 73 }, // sfx_manatk
   { "DSMANSIT", 74 }, // sfx_mansit
   { "DSMNPAIN", 75 }, // sfx_mnpain
   { "DSMANDTH", 76 }, // sfx_mandth
   { "DSSKESIT", 77 }, // sfx_skesit
   { "DSSKEDTH", 78 }, // sfx_skedth
   { "DSSKEACT", 79 }, // sfx_skeact
   { "DSSKEATK", 80 }, // sfx_skeatk
   { "DSSKESWG", 81 }, // sfx_skeswg
   { "DSSKEPCH", 82 }, // sfx_skepch
   { "DSBDOPN",  83 }, // sfx_bdopn
   { "DSBDCLS",  84 }, // sfx_bdcls
   { "DSGETPOW", 85 }, // sfx_getpow
};

// EOF

