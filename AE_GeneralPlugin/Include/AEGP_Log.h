//----------------------------------------------------------------------------
// Copyright Persistant Studios, SARL. All Rights Reserved. https://www.popcornfx.com/terms-and-conditions/
//----------------------------------------------------------------------------
#pragma once

#ifndef	__FX_AEGP_LOG_H__
#define	__FX_AEGP_LOG_H__

#include <AEConfig.h>

#include <entry.h>
#include <AE_GeneralPlug.h>
#include <AE_GeneralPlugPanels.h>
#include <A.h>
#include <SPSuites.h>
#include <AE_Macros.h>

#include "AEGP_Define.h"

//----------------------------------------------------------------------------
namespace AAePk
{
	struct	SAAEIOData;
}

//----------------------------------------------------------------------------

__AEGP_PK_BEGIN

class CAELog
{
	static bool					s_PKState;
	static SAAEIOData			*s_IOData;
public:
	static bool		LogErrorWindows(SAAEIOData *AAEData, const CString errorStr);

	static bool		SetIOData(SAAEIOData *AAEData);
	static void		ClearIOData();
	static bool		TryLogErrorWindows(const CString errorStr);
	static void		SetPKLogState(bool state);


	static bool		TryLogInfoWindows(const CString infoStr);
};

//----------------------------------------------------------------------------

__AEGP_PK_END

#endif

