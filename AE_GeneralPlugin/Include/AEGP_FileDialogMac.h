//----------------------------------------------------------------------------
// Copyright Persistant Studios, SARL. All Rights Reserved. https://www.popcornfx.com/terms-and-conditions/
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
