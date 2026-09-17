//----------------------------------------------------------------------------
// Copyright Persistant Studios, SARL.
// https://popcornfx.com/popcornfx-community-license/
//----------------------------------------------------------------------------
#pragma once

//----------------------------------------------------------------------------

#define	AEPOPCORNFX_BUILD_VERSION	1
// Must be unique & increasing across releases: AE keeps the installed copy with the highest version.
// PF_VERSION() masks minor & patch to 4 bits each (2.8.7 == 2.24.7), so: major << 24 | minor << 16 | patch << 8 | build.
#define AEPOPCORNFX_PIPL_VERSION	35193345
