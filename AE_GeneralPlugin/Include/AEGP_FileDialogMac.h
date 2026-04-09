//----------------------------------------------------------------------------
// Copyright Persistant Studios, SARL.
// https://popcornfx.com/popcornfx-community-license/
//----------------------------------------------------------------------------
#ifndef __AEGP_FILEDIALOGMAC_H__
#define __AEGP_FILEDIALOGMAC_H__

#include "AEGP_Define.h"

__AEGP_PK_BEGIN

//----------------------------------------------------------------------------

#if defined(PK_MACOSX)

CString	OpenFileDialogMac(const TArray<CString> &filters, const CString &defaultPathAndFile = CString());

#endif

//----------------------------------------------------------------------------

__AEGP_PK_END

#endif
